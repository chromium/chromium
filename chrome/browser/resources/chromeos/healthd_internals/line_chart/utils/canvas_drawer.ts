// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BACKGROUND_COLOR, GRID_COLOR, MIN_LABEL_HORIZONTAL_SPACING, MIN_LABEL_VERTICAL_SPACING, MIN_TIME_LABEL_HORIZONTAL_SPACING, SAMPLE_RATE, TEXT_COLOR, TEXT_SIZE, TIME_STEP_UNITS, Y_AXIS_TICK_LENGTH} from '../configs.js';

import {DataSeries} from './data_series.js';
import {UnitLabel} from './unit_label.js';

/**
 * Find the minimum time step for rendering time labels.
 * @param minSpacing - The minimum spacing between two time tick.
 * @param scale - The scale of the line chart.
 */
function getMinimumTimeStep(minSpacing: number, scale: number): number {
  const timeStepUnits: number[] = TIME_STEP_UNITS;
  let timeStep: number = 0;
  for (let i: number = 0; i < timeStepUnits.length; ++i) {
    if (timeStepUnits[i] / scale >= minSpacing) {
      timeStep = timeStepUnits[i];
      break;
    }
  }
  return timeStep;
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

  // See `setMaxValue()`.
  private maxValue: number|null = null;

  // The step size between two data points, in millisecond.
  private stepSize: number = 1;

  // The width and height of the graph for drawing line chart, excluding the
  // bottom labels.
  private graphWidth: number = 1;
  private graphHeight: number = 1;

  // The time to query the data. It will be smaller than the `visibleStartTime`,
  // so the line chart won't leave blanks beside the edge of the chart.
  private queryStartTime: number = 0;

  // The offset of the current visible range. To make sure we draw the data
  // points at the same absolute position.
  private offset: number = 0;

  // Number of the points need to be draw on the screen.
  private numOfPoint: number = 0;

  // Add a data series to this sub chart.
  addDataSeries(dataSeries: DataSeries) {
    this.dataSeriesList.push(dataSeries);
  }

  // Overwrite the maximum value of this sub chart. If this value is not null,
  // the maximum value of the unit label will be set to this value instead of
  // the real maximum value of data series.
  setMaxValue(maxValue: number|null) {
    this.maxValue = maxValue;
    this.updateMaxValue();
  }

  // Return true if there is any data series in this chart.
  shouldRender(): boolean {
    return this.dataSeriesList.length > 0;
  }

  // Render the canvas content.
  renderCanvas(
      context: CanvasRenderingContext2D, canvasWidth: number,
      canvasHeight: number, scrollbarPosition: number, startTime: number,
      scale: number) {
    this.initAndClearContext(context, canvasWidth, canvasHeight);

    this.graphWidth = canvasWidth;
    this.graphHeight = canvasHeight - TEXT_SIZE - MIN_LABEL_VERTICAL_SPACING;

    // To reduce CPU usage, the chart do not draw points at every pixels. Use
    // `offset` to make sure the graph won't shaking during scrolling, the line
    // chart will render the data points at the same absolute position.
    this.offset = scrollbarPosition % SAMPLE_RATE;

    // Draw a data point on every `SAMPLE_RATE` pixels.
    this.stepSize = scale * SAMPLE_RATE;

    // First point's position(`queryStartTime`) may go out of the canvas to make
    // the line chart continuous at the begin of the visible range, as well as
    // the last points.
    const visibleStartTime: number = startTime + scrollbarPosition * scale;
    this.queryStartTime = visibleStartTime - this.offset * scale;
    const queryWidth: number = this.graphWidth + this.offset;
    this.numOfPoint = Math.ceil(queryWidth / SAMPLE_RATE) + 1;

    this.updateMaxValue();
    this.renderChartGrid(context);
    this.renderTimeLabels(context, visibleStartTime, scale);
    this.renderUnitLabels(context);
    this.renderLines(context);
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
      context: CanvasRenderingContext2D, startTime: number, scale: number) {
    const sampleText: string = new Date(startTime).toLocaleTimeString();
    const minSpacing: number = context.measureText(sampleText).width +
        MIN_TIME_LABEL_HORIZONTAL_SPACING;
    const timeStep: number = getMinimumTimeStep(minSpacing, scale);
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
      const xCoord: number = Math.round((time - startTime) / scale);
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

  // Render the lines of all data series.
  renderLines(context: CanvasRenderingContext2D) {
    const dataSeriesList: DataSeries[] = this.dataSeriesList;
    for (let i: number = 0; i < dataSeriesList.length; ++i) {
      this.renderLineOfDataSeries(context, dataSeriesList[i]);
    }
  }

  private renderLineOfDataSeries(
      context: CanvasRenderingContext2D, dataSeries: DataSeries) {
    // Query the the values of data points from the data series.
    const values: Array<number|null> = dataSeries.getDisplayedValues(
        this.queryStartTime, this.stepSize, this.numOfPoint);

    context.strokeStyle = dataSeries.getColor();
    context.fillStyle = dataSeries.getColor();
    context.beginPath();

    const valueScale: number = this.unitLabel.getScale();
    let firstXCoord: number = this.graphWidth;
    let xCoord: number = -this.offset;
    for (let i: number = 0; i < values.length; ++i) {
      if (values[i] !== null) {
        const chartYCoord: number = Math.round(values[i]! * valueScale);
        const realYCoord: number = this.graphHeight - 1 - chartYCoord;
        context.lineTo(xCoord, realYCoord);
        if (firstXCoord > xCoord) {
          firstXCoord = xCoord;
        }
      }
      xCoord += SAMPLE_RATE;
    }
    context.stroke();
    this.fillAreaBelowLine(context, firstXCoord);
  }

  private fillAreaBelowLine(
      context: CanvasRenderingContext2D, firstXCoord: number) {
    context.lineTo(this.graphWidth, this.graphHeight);
    context.lineTo(firstXCoord, this.graphHeight);
    context.globalAlpha = 0.05;
    context.fill();
    context.globalAlpha = 1.0;
  }

  // Render the unit label on the right side of line chart.
  renderUnitLabels(context: CanvasRenderingContext2D) {
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
  private updateMaxValue() {
    const dataSeriesList: DataSeries[] = this.dataSeriesList;
    if (this.maxValue != null) {
      this.unitLabel.setMaxValue(this.maxValue);
      return;
    }
    const valueList: number[] = dataSeriesList.map((dataSeries: DataSeries) => {
      return dataSeries.getMaxValue(
          this.queryStartTime, this.stepSize, this.numOfPoint);
    });
    this.unitLabel.setMaxValue(Math.max(...valueList));
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
