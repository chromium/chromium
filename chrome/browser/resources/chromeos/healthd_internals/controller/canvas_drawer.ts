// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DataPoint} from '../model/data_series.js';
import {BACKGROUND_COLOR, GRID_COLOR, MIN_LABEL_HORIZONTAL_SPACING, MIN_LABEL_VERTICAL_SPACING, MIN_TIME_LABEL_HORIZONTAL_SPACING, TEXT_COLOR, TEXT_SIZE, TIME_STEP_UNITS, Y_AXIS_TICK_LENGTH} from '../utils/line_chart_configs.js';

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
 * Helper class to draw the canvas from data series which share the same unit
 * set. This class is responsible for drawing lines, time labels and unit labels
 * on the line chart.
 */
export class CanvasDrawer {
  // The width and height of the graph for drawing line chart, excluding the
  // labels in canvas footer.
  private graphWidth: number = 1;
  private graphHeight: number = 1;

  private readonly footerHeight = TEXT_SIZE + MIN_LABEL_VERTICAL_SPACING;

  /**
   * Initialize the canvas content.
   *
   * @param context - 2D rendering context for the drawing the line chart.
   * @param canvasWidth - The width of canvas element.
   * @param canvasHeight - The height of canvas element.
   */
  initCanvas(
      context: CanvasRenderingContext2D, canvasWidth: number,
      canvasHeight: number) {
    this.initAndClearContext(context, canvasWidth, canvasHeight);

    this.graphWidth = canvasWidth;
    this.graphHeight = canvasHeight - this.footerHeight;

    this.renderChartGrid(context);
  }

  // Get the height for unit labels displayed in canvas.
  getUnitLabelHeight(): number {
    // Cannot draw the line at the top and the bottom pixel.
    return this.graphHeight - 2;
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

  /**
   * Render the time label under the line chart.
   *
   * @param context - 2D rendering context for the drawing the line chart.
   * @param startTime - The start time of visible part of line chart.
   * @param timeScale - The number of milliseconds between two pixels.
   */
  renderTimeLabels(
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
    const yCoord: number = this.graphHeight + this.footerHeight;
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

  /**
   * Render the line for one data series.
   *
   * @param context - 2D rendering context for the drawing the line chart.
   * @param dataPoints - The data points to be displayed.
   * @param displayedColor - The color of the displayed line.
   * @param startTime - The start time of visible part of line chart.
   * @param timeScale - The number of milliseconds between two pixels.
   * @param valueScale - The real value between two pixels.
   */
  renderLine(
      context: CanvasRenderingContext2D, dataPoints: DataPoint[],
      displayedColor: string, startTime: number, timeScale: number,
      valueScale: number) {
    if (dataPoints.length === 0) {
      return;
    }

    context.strokeStyle = displayedColor;
    context.fillStyle = displayedColor;
    context.beginPath();

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

  /**
   * Render the unit label on the right side of line chart.
   *
   * @param context - 2D rendering context for the drawing the line chart.
   * @param labelTexts - The list of unit labels to displayed.
   */
  renderUnitLabel(context: CanvasRenderingContext2D, labelTexts: string[]) {
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
