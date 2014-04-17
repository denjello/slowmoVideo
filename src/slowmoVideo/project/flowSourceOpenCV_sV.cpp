/*
This file is part of slowmoVideo.
Copyright (C) 2012  Lucas Walter
              2012  Simon A. Eugster (Granjow)  <simon.eu@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "flowSourceOpenCV_sV.h"
#include "project_sV.h"
#include "abstractFrameSource_sV.h"
#include "../lib/flowRW_sV.h"
#include "../lib/flowField_sV.h"

#include "opencv2/video/tracking.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"
#include <QtCore/QTime>
#include <iostream>
#include <fstream>
using namespace cv;

FlowSourceOpenCV_sV::FlowSourceOpenCV_sV(Project_sV *project) :
    AbstractFlowSource_sV(project)
{
    createDirectories();
}

void FlowSourceOpenCV_sV::slotUpdateProjectDir()
{
    m_dirFlowSmall.rmdir(".");
    m_dirFlowOrig.rmdir(".");
    createDirectories();
}

void FlowSourceOpenCV_sV::createDirectories()
{
    m_dirFlowSmall = project()->getDirectory("cache/oFlowSmall");
    m_dirFlowOrig = project()->getDirectory("cache/oFlowOrig");
}




// TODO: check usage of cflowmap ? create a branch ?
void drawOptFlowMap(const Mat& flow,  int step,
                    double, const Scalar& color, std::string flowname )
{

  cv::Mat log_flow, log_flow_neg;
  //log_flow = cv::abs( flow/3.0 );
  cv::log(cv::abs(flow)*3 + 1, log_flow);
  cv::log(cv::abs(flow*(-1.0))*3 + 1, log_flow_neg);
  const float scale = 64.0;
  const float offset = 128.0;

  float max_flow = 0.0;

  FlowField_sV flowField(flow.cols, flow.rows);

    for(int y = 0; y < flow.rows; y += step)
        for(int x = 0; x < flow.cols; x += step)
        {
            const Point2f& fxyo = flow.at<Point2f>(y, x);

            flowField.setX(x, y, fxyo.x);
            flowField.setY(x, y, fxyo.y);
        }

  FlowRW_sV::save(flowname, &flowField);
}

const QString FlowSourceOpenCV_sV::flowPath(const uint leftFrame, const uint rightFrame, const FrameSize frameSize) const
{
    QDir dir;
    if (frameSize == FrameSize_Orig) {
        dir = m_dirFlowOrig;
    } else {
        dir = m_dirFlowSmall;
    }
    QString direction;
    if (leftFrame < rightFrame) {
        direction = "forward";
    } else {
        direction = "backward";
    }

    return dir.absoluteFilePath(QString("ocv-%1-%2-%3.sVflow").arg(direction).arg(leftFrame).arg(rightFrame));
}
FlowField_sV* FlowSourceOpenCV_sV::buildFlow(uint leftFrame, uint rightFrame, FrameSize frameSize) throw(FlowBuildingError)
{
    QString flowFileName(flowPath(leftFrame, rightFrame, frameSize));

    /// \todo Check if size is equal
    if (!QFile(flowFileName).exists()) {

        QTime time;
        time.start();

        Mat prevgray, gray, flow, cflow;
        QString prevpath = project()->frameSource()->framePath(leftFrame, frameSize);
        QString path = project()->frameSource()->framePath(rightFrame, frameSize);
//        namedWindow("flow", 1);

		qDebug() << "Building flow for left frame " << prevpath << " to right frame " << path ;
		qDebug() << "Building flow for left frame " << leftFrame << " to right frame " << rightFrame << "; Size: " << frameSize;
		
        prevgray = imread(prevpath.toStdString(), 0);
        gray = imread(path.toStdString(), 0);

        //cvtColor(l1, prevgray, CV_BGR2GRAY);
        //cvtColor(l2, gray, CV_BGR2GRAY);

        {

            if( prevgray.data ) {
                // TBD need sliders for all these parameters
	      const int levels = 3; // 5
            const int winsize = 15; // 13
            const int iterations = 8; // 10

            const double polySigma = 1.2;
            const double pyrScale = 0.5;
            const int polyN = 5;
            const int flags = 0;

                calcOpticalFlowFarneback(
                    prevgray, gray,
                    //gray, prevgray,  // TBD this seems to match V3D output better but a sign flip could also do that
                    flow,
                    pyrScale, //0.5,
                    levels, //3,
                    winsize, //15,
                    iterations, //3,
                    polyN, //5,
                    polySigma, //1.2,
                    flags //0
                    );
                //cvtColor(prevgray, cflow, CV_GRAY2BGR);
                //drawOptFlowMap(flow, cflow, 16, 1.5, CV_RGB(0, 255, 0));
                drawOptFlowMap(flow,  1, 1.5, CV_RGB(0, 255, 0), flowFileName.toStdString());
                //imshow("flow", cflow);
                //imwrite(argv[4],cflow);
            } else {
                qDebug() << "imread: Could not read image " << prevpath;
                throw FlowBuildingError(QString("imread: Could not read image " + prevpath));
            }
        }

        qDebug() << "Optical flow built for " << flowFileName << " in " << time.elapsed() << " ms.";

    } else {
        qDebug().nospace() << "Re-using existing flow image for left frame " << leftFrame << " to right frame " << rightFrame << ": " << flowFileName;
    }

    try {
        return FlowRW_sV::load(flowFileName.toStdString());
    } catch (FlowRW_sV::FlowRWError &err) {
        throw FlowBuildingError(err.message.c_str());
    }
}



