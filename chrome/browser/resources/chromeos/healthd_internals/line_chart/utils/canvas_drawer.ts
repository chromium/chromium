// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BACKGROUND_COLOR, GRID_COLOR, MIN_LABEL_HORIZONTAL_SPACING, MIN_LABEL_VERTICAL_SPACING, MIN_TIME_LABEL_HORIZONTAL_SPACING, MIN_TIME_SCALE, SAMPLE_RATE, TEXT_COLOR, TEXT_SIZE, TIME_STEP_UNITS, Y_AXIS_TICK_LENGTH} from '../configs.js';

import type {DataPoint} from './data_series.js';
import {DataSeries} from './data_series.js';
import {UnitLabel} from './unit_label.js';

/**
 * Find the minimum time step for rendering time labels.
 *
 * @param minSpacing - The minimum spacing between two time tick.
 * @param timeScale - The horizontal scale of the line chart.
 */
function getMinimumTimeStep(minSpacing: number, timeScale: number): number {
  const timeStepUnits: number[] = TIME_STEP_UNITS;
  let timeStep: number = 0;
  for (let i: number = 0; i < timeStepUnits.length; ++i) {
    if (timeStepUnits[i] / timeScale >= minSpacing) {
      timeStep = timeStepUnits[i];
      break;
    }
  }
  return timeStep;
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
 * Helper class to draw the canvas from data series which share the same unit
 * set. This class is responsible for drawing lines, time labels and unit labels
 * on the line chart.
 */
export class CanvasDrawer {
  constructor(units: string[], unitBase: number) {
    this.unitLabel = new UnitLabel(units, unitBase);
  }

  // Set in constructor.
  private readonly unitLabel: UnitLabel;

  // List of displayed data.
  private dataSeriesList: DataSeries[] = [];

  // The fixed maximum value in line chart. If this value is null, the maximum
  // value of unit label will be set from the real maximum value of data series.
  private fixedMaxValue: number|null = null;

  // The width and height of the graph for drawing line chart, excluding the
  // bottom labels.
  private graphWidth: number = 1;
  private graphHeight: number = 1;

  // Add a data series to this sub chart.
  addDataSeries(dataSeries: DataSeries) {
    this.dataSeriesList.push(dataSeries);
  }

  // Overwrite the maximum value of this chart.
  setFixedMaxValue(maxValue: number|null) {
    this.fixedMaxValue = maxValue;
  }

  // Return true if there is any data series in this chart.
  shouldRender(): boolean {
    return this.dataSeriesList.length > 0;
  }

  /**
   * Render the canvas content.
   *
   * @param context - 2D rendering context for the drawing the line chart.
   * @param canvasWidth - The width of canvas element.
   * @param canvasHeight - The height of canvas element.
   * @param visibleStartTime - The start time of visible part of line chart.
   * @param visibleEndTime - The end time of visible part of line chart.
   * @param timeScale - The number of milliseconds between two pixels.
   */
  renderCanvas(
      context: CanvasRenderingContext2D, canvasWidth: number,
      canvasHeight: number, visibleStartTime: number, visibleEndTime: number,
      timeScale: number) {
    this.initAndClearContext(context, canvasWidth, canvasHeight);

    this.graphWidth = canvasWidth;
    this.graphHeight = canvasHeight - TEXT_SIZE - MIN_LABEL_VERTICAL_SPACING;

    this.renderChartGrid(context);
    this.renderTimeLabels(context, visibleStartTime, timeScale);

    const stepSize: number = getStepSize(timeScale);
    const maxValue: number =
        this.getVisibleMaxValue(visibleStartTime, visibleEndTime, stepSize);
    this.renderUnitLabel(context, maxValue);
    this.renderLines(
        context, visibleStartTime, visibleEndTime, timeScale, stepSize);
  }

  private initAndClearContext(
      context: CanvasRenderingContext2D, canvasWidth: number,
      canvasHeight: number) {
    context.font = `${TEXT_SIZE}px Arial`;
    context.lineWidth = 2;
    context.lineCap = 'round';
    context.lineJoin = 'round';
    context.fillStyle = BACKGROUND_COLOR;
    context.fillRect(0, 0, canvasWidth, canvasHeight);
  }

  private renderChartGrid(context: CanvasRenderingContext2D) {
    context.strokeStyle = GRID_COLOR;
    context.strokeRect(0, 0, this.graphWidth - 1, this.graphHeight);
  }

  // Render the time label under the line chart.
  private renderTimeLabels(
      context: CanvasRenderingContext2D, startTime: number, timeScale: number) {
    const sampleText: string = new Date(startTime).toLocaleTimeString();
    const minSpacing: number = context.measureText(sampleText).width +
        MIN_TIME_LABEL_HORIZONTAL_SPACING;
    const timeStep: number = getMinimumTimeStep(minSpacing, timeScale);
    if (timeStep === 0) {
      console.warn('Render time label failed. Cannot find minimum time unit.');
      return;
    }

    context.textBaseline = 'bottom';
    context.textAlign = 'center';
    context.fillStyle = TEXT_COLOR;
    context.strokeStyle = GRID_COLOR;
    context.beginPath();
    const yCoord: number =
        this.graphHeight + TEXT_SIZE + MIN_LABEL_VERTICAL_SPACING;
    const firstTimeTick: number = Math.ceil(startTime / timeStep) * timeStep;
    let time: number = firstTimeTick;
    while (true) {
      const xCoord: number = Math.round((time - startTime) / timeScale);
      if (xCoord >= this.graphWidth) {
        break;
      }
      const text: string = new Date(time).toLocaleTimeString();
      context.fillText(text, xCoord, yCoord);
      context.moveTo(xCoord, 0);
      context.lineTo(xCoord, this.graphHeight - 1);
      time += timeStep;
    }
    context.stroke();
  }

  // Render lines for all data series.
  private renderLines(
      context: CanvasRenderingContext2D, startTime: number, endTime: number,
      timeScale: number, stepSize: number) {
    for (const dataSeries of this.dataSeriesList) {
      this.renderLine(
          context, dataSeries, startTime, endTime, timeScale, stepSize);
    }
  }

  // Render the line for one data series.
  private renderLine(
      context: CanvasRenderingContext2D, dataSeries: DataSeries,
      startTime: number, endTime: number, timeScale: number, stepSize: number) {
    // Query the the values of data points from the data series.
    const dataPoints: DataPoint[] =
        dataSeries.getDisplayedPoints(startTime, endTime, stepSize);
    if (dataPoints.length === 0) {
      return;
    }

    context.strokeStyle = dataSeries.getColor();
    context.fillStyle = dataSeries.getColor();
    context.beginPath();

    const valueScale: number = this.unitLabel.getValueScale();
    for (const point of dataPoints) {
      const xCoord: number = Math.round((point.time - startTime) / timeScale);
      const chartYCoord: number = Math.round(point.value / valueScale);
      const realYCoord: number = this.graphHeight - 1 - chartYCoord;
      context.lineTo(xCoord, realYCoord);
    }
    context.stroke();
    const firstXCoord: number =
        Math.round((dataPoints[0].time - startTime) / timeScale);
    const lastXCoord: number = Math.round(
        (dataPoints[dataPoints.length - 1].time - startTime) / timeScale);
    this.fillAreaBelowLine(context, firstXCoord, lastXCoord);
  }

  private fillAreaBelowLine(
      context: CanvasRenderingContext2D, firstXCoord: number,
      lastXCoord: number) {
    context.lineTo(lastXCoord, this.graphHeight);
    context.lineTo(firstXCoord, this.graphHeight);
    context.globalAlpha = 0.05;
    context.fill();
    context.globalAlpha = 1.0;
  }

  // Render the unit label on the right side of line chart.
  private renderUnitLabel(context: CanvasRenderingContext2D, maxValue: number) {
    this.unitLabel.setMaxValue(maxValue);

    // Cannot draw the line at the top and the bottom pixel.
    const labelHeight: number = this.graphHeight - 2;
    this.unitLabel.setLayout(labelHeight, /* precision */ 2);

    const labelTexts: string[] = this.unitLabel.getLabels();
    if (labelTexts.length === 0) {
      return;
    }

    context.textAlign = 'right';
    const tickStartX: number = this.graphWidth - 1;
    const tickEndX: number = this.graphWidth - 1 - Y_AXIS_TICK_LENGTH;
    const textXCoord: number = this.graphWidth - MIN_LABEL_HORIZONTAL_SPACING;
    const labelYStep: number = this.graphHeight / (labelTexts.length - 1);

    this.renderLabelTicks(
        context, labelTexts, labelYStep, tickStartX, tickEndX);
    this.renderLabelTexts(context, labelTexts, labelYStep, textXCoord);
  }

  // Calculate the max value for the current layout of unit label.
  private getVisibleMaxValue(
      startTime: number, endTime: number, stepSize: number): number {
    if (this.fixedMaxValue != null) {
      return this.fixedMaxValue;
    }
    return this.dataSeriesList.reduce(
        (maxValue, item) => Math.max(
            maxValue, item.getDisplayedMaxValue(startTime, endTime, stepSize)),
        0);
  }

  // Render the tick line for the unit label.
  private renderLabelTicks(
      context: CanvasRenderingContext2D, labelTexts: string[],
      labelYStep: number, tickStartX: number, tickEndX: number) {
    context.strokeStyle = GRID_COLOR;
    context.beginPath();
    // First and last tick are the top and the bottom of the line chart, so
    // don't draw them again. */
    for (let i: number = 1; i < labelTexts.length - 1; ++i) {
      const yCoord: number = labelYStep * i;
      context.moveTo(tickStartX, yCoord);
      context.lineTo(tickEndX, yCoord);
    }
    context.stroke();
  }

  // Render the texts for the unit label.
  private renderLabelTexts(
      context: CanvasRenderingContext2D, labelTexts: string[],
      labelYStep: number, textXCoord: number) {
    // The first label cannot align the bottom of the tick or it will go outside
    // the canvas.
    context.fillStyle = TEXT_COLOR;
    context.textBaseline = 'top';
    context.fillText(labelTexts[0], textXCoord, 0);

    context.textBaseline = 'bottom';
    for (let i: number = 1; i < labelTexts.length; ++i) {
      const yCoord: number = labelYStep * i;
      context.fillText(labelTexts[i], textXCoord, yCoord);
    }
  }
}
