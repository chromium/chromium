// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

import type {DataPoint, DataSeries} from '../model/data_series.js';
import {MIN_TIME_SCALE, SAMPLE_RATE} from '../utils/line_chart_configs.js';
import type {HealthdInternalsLineChartElement} from '../view/line_chart/line_chart.js'

import {CanvasDrawer} from './canvas_drawer.js';
import type {DataSeriesList} from './system_trend_controller.js';
import {CategoryTypeEnum} from './system_trend_controller.js';
import {UnitLabel} from './unit_label.js';

/**
 * Get the step size based on the current time scale. We will only display one
 * point in one step.
 *
 * @param timeScale - The horizontal scale of the line chart.
 * @returns - The step size in millisecond.
 */
function getStepSize(timeScale: number): number {
  const baseStepSize: number = MIN_TIME_SCALE * SAMPLE_RATE;
  const minStepSize: number = timeScale * SAMPLE_RATE;
  const minExponent = Math.log(minStepSize / baseStepSize) / Math.LN2;
  // Round up to the next `baseStepSize` multiplied by the power of 2 to avoid
  // shaking during zooming.
  return baseStepSize * Math.pow(2, Math.ceil(minExponent));
}

/**
 * Controller for line chart.
 */
export class LineChartController {
  constructor(element: HealthdInternalsLineChartElement) {
    this.element = element;
  }

  // The corresponding Polymer element.
  private element: HealthdInternalsLineChartElement;

  // The helper class to draw the canvas.
  private canvasDrawer: CanvasDrawer = new CanvasDrawer();

  // Used to avoid updating the graph multiple times in a single operation.
  private chartUpdateTimer: number = 0;

  // The start and end time of the data source in the line chart. (Unix time)
  private startTime: number = Date.now();
  private endTime: number = this.startTime;

  // The current displayed category.
  private displayedCategory: CategoryTypeEnum;

  // The list of data series displayed in the line chart.
  private displayeDataSeriesList?: DataSeriesList;

  // The fixed maximum value in line chart. If this value is null, the maximum
  // value of unit label will be set from the real maximum value of data series.
  private fixedMaxValue: number|null = null;

  // Set up the list of data series.
  setupDataSeriesList(
      category: CategoryTypeEnum, dataSeriesList: DataSeriesList) {
    if (category === this.displayedCategory) {
      return;
    }

    this.displayedCategory = category;
    this.displayeDataSeriesList = dataSeriesList;

    if (this.displayedCategory === CategoryTypeEnum.CPU_USAGE) {
      this.fixedMaxValue = 100;
    } else {
      this.fixedMaxValue = null;
    }
  }

  // Get the scrollbale range for the scrollbar.
  getScrollableRange(canvasWidth: number, timeScale: number): number {
    return Math.max(this.getChartWidth(timeScale) - canvasWidth, 0);
  }

  // Render the lines on canvas. Note that to avoid calling render function
  // multiple times in a single operation, this function will set a timeout
  // rather than calling render function directly.
  updateCanvas(
      context: CanvasRenderingContext2D, canvasWidth: number,
      canvasHeight: number, timeScale: number, scrollbarPosition: number) {
    clearTimeout(this.chartUpdateTimer);
    if (this.displayeDataSeriesList === undefined ||
        this.displayeDataSeriesList.dataList.length === 0) {
      return;
    }
    const data = this.displayeDataSeriesList;
    this.chartUpdateTimer = setTimeout(
        () => this.renderCanvas(
            context, canvasWidth, canvasHeight, timeScale, scrollbarPosition,
            data.unitLabel, data.dataList));
  }

  // Get the whole line chart width, in pixel.
  private getChartWidth(timeScale: number): number {
    const timeRange: number = this.endTime - this.startTime;
    return Math.floor(timeRange / timeScale);
  }

  // Render the canvas by `canvasDrawer`.
  private renderCanvas(
      context: CanvasRenderingContext2D, canvasWidth: number,
      canvasHeight: number, timeScale: number, scrollbarPosition: number,
      unitLabel: UnitLabel, dataSeriesList: DataSeries[]) {
    assert(dataSeriesList.length !== 0);
    this.updateDataTime(dataSeriesList);

    // To reduce CPU usage, only visible part of chart will be draw on canvas.
    // We need to know the offset of data from `scrollbarPosition`.
    if (this.getScrollableRange(canvasWidth, timeScale) === 0) {
      // If the chart width less than the canvas width, make the chart align
      // right by setting the negative position.
      scrollbarPosition = this.getChartWidth(timeScale) - canvasWidth;
    }

    const visibleStartTime = this.startTime + scrollbarPosition * timeScale;
    const visibleEndTime = visibleStartTime + canvasWidth * timeScale;
    this.canvasDrawer.initCanvas(context, canvasWidth, canvasHeight);
    this.canvasDrawer.renderTimeLabels(context, visibleStartTime, timeScale);

    const stepSize = getStepSize(timeScale);
    const maxValue = this.getVisibleMaxValue(
        dataSeriesList, visibleStartTime, visibleEndTime, stepSize);
    unitLabel.setMaxValue(maxValue);
    unitLabel.setLayout(this.canvasDrawer.getUnitLabelHeight(), 2);
    this.canvasDrawer.renderUnitLabel(context, unitLabel.getLabels());

    for (const dataSeries of dataSeriesList) {
      // Query the the values of data points from the data series.
      const dataPoints: DataPoint[] = dataSeries.getDisplayedPoints(
          visibleStartTime, visibleEndTime, stepSize);
      if (dataPoints.length === 0) {
        continue;
      }
      this.canvasDrawer.renderLine(
          context, dataPoints, dataSeries.getColor(), visibleStartTime,
          timeScale, unitLabel.getValueScale());
    }

    this.element.getSummaryTable().updateDisplayedInfo(
        visibleStartTime, visibleEndTime, unitLabel.getCurrentUnitString(),
        unitLabel.getCurrentUnitScale());
  }

  private updateDataTime(dataSeriesList: DataSeries[]) {
    let latestStartTime = Number.MAX_SAFE_INTEGER;
    for (const dataSeries of dataSeriesList) {
      const points = dataSeries.getPoints();
      const length = points.length;
      if (length !== 0) {
        latestStartTime = Math.min(latestStartTime, points[0].time);
        this.endTime = Math.max(this.endTime, points[length - 1].time);
      }
    }
    this.startTime = Math.max(this.startTime, latestStartTime);
  }

  // Calculate the max value for the current layout of unit label.
  private getVisibleMaxValue(
      dataSeriesList: DataSeries[], visibleStartTime: number,
      visibleEndTime: number, stepSize: number): number {
    if (this.fixedMaxValue != null) {
      return this.fixedMaxValue;
    }
    return dataSeriesList.reduce(
        (maxValue, item) => Math.max(
            maxValue,
            item.getDisplayedMaxValue(
                visibleStartTime, visibleEndTime, stepSize)),
        0);
  }
}
