// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BACKGROUND_COLOR, TEXT_SIZE} from '../configs.js';

import {DataSeries} from './data_series.js';
import {UnitLabel} from './unit_label.js';

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

  // Add a data series to this sub chart.
  addDataSeries(dataSeries: DataSeries) {
    this.dataSeriesList.push(dataSeries);
  }

  // Overwrite the maximum value of this sub chart. If this value is not null,
  // the maximum value of the unit label will be set to this value instead of
  // the real maximum value of data series.
  setMaxValue(maxValue: number|null) {
    this.maxValue = maxValue;
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
    // TODO(b/350423216): Render the canvas content.
    console.info(scrollbarPosition, startTime, scale);
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
}
