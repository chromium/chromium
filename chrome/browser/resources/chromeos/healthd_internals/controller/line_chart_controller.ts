// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

import type {DataSeries} from '../model/data_series.js';
import {LINE_CHART_COLOR_SET, MIN_TIME_SCALE, SAMPLE_RATE} from '../utils/line_chart_configs.js';
import type {DisplayedLineInfo} from '../view/line_chart/chart_summary_table.js';
import type {HealthdInternalsLineChartElement} from '../view/line_chart/line_chart.js';

import {CanvasDrawer} from './canvas_drawer.js';
import type {DataSeriesList} from './system_trend_controller.js';
import {CategoryTypeEnum} from './system_trend_controller.js';
import {UnitLabel} from './unit_label.js';

export function getLineChartColor(index: number) {
  const colorIdx: number = index % LINE_CHART_COLOR_SET.length;
  return LINE_CHART_COLOR_SET[colorIdx];
}

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

  // The lists of data series from different sources.
  private displayedDataSeriesLists: DataSeriesList[] = [];

  // The fixed maximum value in line chart. If this value is null, the maximum
  // value of unit label will be set from the real maximum value of data series.
  private fixedMaxValue: number|null = null;

  // Set up the lists of data series.
  setupDataSeries(
      category: CategoryTypeEnum, dataSeriesLists: DataSeriesList[]) {
    this.displayedCategory = category;
    this.displayedDataSeriesLists = dataSeriesLists;

    if (this.displayedCategory === CategoryTypeEnum.CPU_USAGE) {
      this.fixedMaxValue = 100;
    } else {
      this.fixedMaxValue = null;
    }
  }

  // Update the start time and end time of data.
  updateDataTime() {
    let newStartTime = Number.MAX_SAFE_INTEGER;
    for (const data of this.displayedDataSeriesLists) {
      for (const dataSeries of data.dataList) {
        const points = dataSeries.getPoints();
        const length = points.length;
        if (length !== 0) {
          newStartTime = Math.min(newStartTime, points[0].time);
          this.endTime = Math.max(this.endTime, points[length - 1].time);
        }
      }
    }

    if (newStartTime !== Number.MAX_SAFE_INTEGER) {
      this.startTime = Math.max(this.startTime, newStartTime);
    }
  }

  // Get the scrollbale range for the scrollbar.
  getScrollableRange(canvasWidth: number, timeScale: number): number {
    return Math.max(this.getChartWidth(timeScale) - canvasWidth, 0);
  }

  // Get the whole line chart width, in pixel.
  private getChartWidth(timeScale: number): number {
    const timeRange: number = this.endTime - this.startTime;
    return Math.floor(timeRange / timeScale);
  }

  // Render the lines on canvas. Note that to avoid calling render function
  // multiple times in a single operation, this function will set a timeout
  // rather than calling render function directly.
  updateCanvas(
      context: CanvasRenderingContext2D, canvasWidth: number,
      canvasHeight: number, timeScale: number, scrollbarPosition: number) {
    clearTimeout(this.chartUpdateTimer);
    this.chartUpdateTimer = setTimeout(
        () => this.renderCanvas(
            context, canvasWidth, canvasHeight, timeScale, scrollbarPosition));
  }

  // Render the canvas by `canvasDrawer`.
  private renderCanvas(
      context: CanvasRenderingContext2D, canvasWidth: number,
      canvasHeight: number, timeScale: number, scrollbarPosition: number) {
    this.canvasDrawer.initCanvas(context, canvasWidth, canvasHeight);
    if (this.displayedDataSeriesLists.length === 0) {
      console.warn('LineChartController: Empty data.');
      return;
    }

    // To reduce CPU usage, only visible part of chart will be draw on canvas.
    // We need to know the offset of data from `scrollbarPosition`.
    if (this.getScrollableRange(canvasWidth, timeScale) === 0) {
      // If the chart width less than the canvas width, make the chart align
      // right by setting the negative position.
      scrollbarPosition = this.getChartWidth(timeScale) - canvasWidth;
    }

    const visibleStartTime = this.startTime + scrollbarPosition * timeScale;
    const visibleEndTime = visibleStartTime + canvasWidth * timeScale;
    this.canvasDrawer.renderTimeLabels(context, visibleStartTime, timeScale);
    const stepSize = getStepSize(timeScale);

    if (this.displayedDataSeriesLists.length === 1) {
      this.renderSingleDataSource(
          context, visibleStartTime, visibleEndTime, stepSize, timeScale);
    } else {
      this.renderMultipleDataSource(
          context, visibleStartTime, visibleEndTime, stepSize, timeScale);
    }

    this.updateSummaryTable(visibleStartTime, visibleEndTime, stepSize);
  }

  /**
   * Renders data from a single data source on the canvas. It calculates the
   * y-axis value from the raw data by applying two scaling factors:
   *
   *  - `unitScale`: Divides the raw data to convert it to the displayed units
   *                 (e.g., bytes to kilobytes, where unitScale would be 1024).
   *  - `pixelScale`: Divides the displayed value to convert it to the
   *                  corresponding height in pixels on the y-axis of the chart.
   *
   * The `valueScale`, used for plotting raw data on y-axis, is calculated as:
   *  - `valueScale` = `unitScale` * `pixelScale`
   *
   * The function also renders unit labels on the y-axis by the corresponding
   * `UnitLabel`.
   */
  private renderSingleDataSource(
      context: CanvasRenderingContext2D, visibleStartTime: number,
      visibleEndTime: number, stepSize: number, timeScale: number) {
    assert(this.displayedDataSeriesLists.length === 1);

    const dataSeriesList = this.displayedDataSeriesLists[0].dataList;
    const unitLabel = this.displayedDataSeriesLists[0].unitLabel;
    const maxValue = this.getVisibleMaxValue(
        dataSeriesList, visibleStartTime, visibleEndTime, stepSize);
    unitLabel.setMaxValue(maxValue);
    unitLabel.setLayout(this.canvasDrawer.getUnitLabelHeight());
    this.canvasDrawer.renderUnitLabel(context, unitLabel.getLabels());

    for (const [index, dataSeries] of dataSeriesList.entries()) {
      const dataPoints = dataSeries.getDisplayedPoints(
          visibleStartTime, visibleEndTime, stepSize);
      this.canvasDrawer.renderLine(
          context, dataPoints, getLineChartColor(index), visibleStartTime,
          timeScale, unitLabel.getValueScale());
    }
  }

  /**
   * Renders data from multiple data sources on the canvas. It calculates the
   * y-axis value from the raw data by applying two scaling factors:
   *
   *  - `normalizationScale`: Normalizes the raw values to a range between 0 and
   *                          1 by dividing them by the minimum power of 2 that
   *                          exceeds the maximum displayed value.
   *  - `pixelScale`: Divides the normalized value to convert it to the
   *                  corresponding height in pixels on the y-axis of the chart.
   *
   * The `valueScale`, used for plotting raw data on y-axis, is calculated as:
   *  - `valueScale` = `normalizationScale` * `pixelScale`
   *
   * This function also renders unit labels without unit strings on the y-axis
   * by a shared `UnitLabel` with a maximum value of 1.
   */
  private renderMultipleDataSource(
      context: CanvasRenderingContext2D, visibleStartTime: number,
      visibleEndTime: number, stepSize: number, timeScale: number) {
    assert(this.displayedDataSeriesLists.length >= 1);

    const normalizedUpperBound = 1;
    const pixelScale =
        normalizedUpperBound / this.canvasDrawer.getUnitLabelHeight();

    const sharedUnitLabel = new UnitLabel([''], 1);
    sharedUnitLabel.setMaxValue(normalizedUpperBound);
    sharedUnitLabel.setLayout(this.canvasDrawer.getUnitLabelHeight());
    this.canvasDrawer.renderUnitLabel(context, sharedUnitLabel.getLabels());

    let colorIndex = 0;
    for (const data of this.displayedDataSeriesLists) {
      const maxValue = this.getVisibleMaxValue(
          data.dataList, visibleStartTime, visibleEndTime, stepSize);
      data.unitLabel.setMaxValue(maxValue);
      data.unitLabel.setLayout(this.canvasDrawer.getUnitLabelHeight());

      const valueScale = this.getNormalizationScale(maxValue) * pixelScale;
      for (const dataSeries of data.dataList) {
        const dataPoints = dataSeries.getDisplayedPoints(
            visibleStartTime, visibleEndTime, stepSize);
        this.canvasDrawer.renderLine(
            context, dataPoints, getLineChartColor(colorIndex),
            visibleStartTime, timeScale, valueScale);
        colorIndex += 1;
      }
    }
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

  // The normalization scale is the minimum power of 2 that exceeds the maximum
  // displayed value.
  private getNormalizationScale(maxValue: number): number {
    return Math.pow(2, Math.ceil(Math.log(maxValue) / Math.log(2)));
  }

  // Get the required info for summary table.
  private updateSummaryTable(
      visibleStartTime: number, visibleEndTime: number, stepSize: number) {
    const output: DisplayedLineInfo[] = [];
    let colorIndex = 0;
    for (const data of this.displayedDataSeriesLists) {
      const unitScale = data.unitLabel.getUnitScale();
      const unitString = data.unitLabel.getUnitString();
      for (const dataSeries of data.dataList) {
        const statistics =
            dataSeries.getLatestStatistics(visibleStartTime, visibleEndTime);
        const info: DisplayedLineInfo = {
          legendColor: getLineChartColor(colorIndex),
          name: dataSeries.getTitle(),
          isVisible: dataSeries.getVisible(),
          displayedUnit: unitString,
          latestValue: statistics.latest / unitScale,
          minValue: statistics.min / unitScale,
          maxValue: statistics.max / unitScale,
          averageValue: statistics.average / unitScale,
        };
        if (this.displayedCategory === CategoryTypeEnum.CUSTOM) {
          info.name = dataSeries.getTitleForCustom();
          const maxValue = this.getVisibleMaxValue(
              data.dataList, visibleStartTime, visibleEndTime, stepSize);
          const normalizationScale = this.getNormalizationScale(maxValue);
          info.normalizationWeight = unitScale / normalizationScale;
          info.normalizedValue = statistics.latest / normalizationScale;
        }
        output.push(info);
        colorIndex += 1;
      }
    }
    this.element.getSummaryTable().updateSummaryInfo(output);
    this.element.updateVisibleTimeSpan(visibleStartTime, visibleEndTime);
  }
}
