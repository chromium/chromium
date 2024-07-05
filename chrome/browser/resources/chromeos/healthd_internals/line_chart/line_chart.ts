// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/iron-location/iron-location.js';
import '//resources/polymer/v3_0/iron-pages/iron-pages.js';
import './menu.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DEFAULT_SCALE} from './configs.js';
import {getTemplate} from './line_chart.html.js';
import type {HealthdInternalsLineChartMenuElement} from './menu.js';
import {CanvasDrawer} from './utils/canvas_drawer.js';
import {DataSeries} from './utils/data_series.js';

export interface HealthdInternalsLineChartElement {
  $: {
    chartRoot: HTMLElement,
    mainCanvas: HTMLCanvasElement,
    chartMenu: HealthdInternalsLineChartMenuElement,
  };
}

/**
 * Create a line chart canvas, including a left button menu and a bottom
 * scrollbar. This element will enroll the events of the line chart, handle the
 * scroll, touch and resize events, and render canvas by `canvasDrawer`.
 */
export class HealthdInternalsLineChartElement extends PolymerElement {
  static get is() {
    return 'healthd-internals-line-chart';
  }

  static get template() {
    return getTemplate();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.startTime = Date.now();
    this.endTime = this.startTime + 1;

    const resizeObserver = new ResizeObserver(() => {
      this.resizeChart();
      this.updateChart();
    });
    resizeObserver.observe(this.$.chartRoot);

    window.addEventListener('menu-buttons-updated', () => {
      this.updateChart();
    });
  }

  // The start time of the time line chart. (Unix time)
  private startTime: number;
  // The end time of the time line chart. (Unix time)
  private endTime: number;

  // The scale of the line chart. Milliseconds per pixel.
  private scale: number = DEFAULT_SCALE;

  // The helper class to draw the canvas.
  private canvasDrawer: CanvasDrawer|null = null;

  // Used to avoid updating the graph multiple times in a single operation.
  private chartUpdateTimer: number = 0;

  // Initialize the `canvasDrawer`.
  initCanvasDrawer(units: string[], unitBase: number) {
    this.canvasDrawer = new CanvasDrawer(units, unitBase);
    this.updateChart();
  }

  // Add a data series to the line chart. Call `initCanvasDrawer()` before
  // calling this function.
  addDataSeries(dataSeries: DataSeries) {
    if (this.canvasDrawer === null) {
      console.warn(
          'This `canvasDrawer` has not been initilaized yet. Call ',
          '`initCanvasDrawer()` before calling this function.');
      return;
    }
    this.canvasDrawer.addDataSeries(dataSeries);
    this.$.chartMenu.addDataSeries(dataSeries);
    this.resizeChart();
    this.updateChart();
  }

  // Update the end time of the line chart. Also update the line chart to
  // display latest data.
  updateEndTime(endTime: number) {
    if (this.canvasDrawer === null) {
      console.warn(
          'This `canvasDrawer` has not been initilaized yet. Call ',
          '`initCanvasDrawer()` before calling this function.');
      return;
    }
    this.endTime = endTime;
    this.updateChart();
  }

  // Overwrite the maximum value of the chart.
  setChartMaxValue(maxValue: number|null) {
    if (this.canvasDrawer === null) {
      console.warn(
          'This `canvasDrawer` has not been initilaized yet. Call ',
          '`initCanvasDrawer()` before calling this function.');
      return;
    }
    this.canvasDrawer.setMaxValue(maxValue);
  }

  private resizeChart() {
    const width = this.getChartVisibleWidth();
    const height = this.getChartVisibleHeight();
    if (this.$.mainCanvas.width === width &&
        this.$.mainCanvas.height === height) {
      return;
    }

    this.$.mainCanvas.width = width;
    this.$.mainCanvas.height = height;
  }

  // Get the visible chart width.
  private getChartVisibleWidth(): number {
    return this.$.chartRoot.offsetWidth - this.$.chartMenu.getWidth();
  }

  // Get the visible chart height.
  private getChartVisibleHeight(): number {
    return this.$.chartRoot.offsetHeight;
  }

  // Render the line chart. Note that to avoid calling render function
  // multiple times in a single operation, this function will set a timeout
  // rather than calling render function directly.
  private updateChart() {
    clearTimeout(this.chartUpdateTimer);
    const context: CanvasRenderingContext2D|null =
        this.$.mainCanvas.getContext('2d');
    if (context === null || this.canvasDrawer === null ||
        !this.canvasDrawer.shouldRender()) {
      return;
    }
    this.chartUpdateTimer = setTimeout(() => this.renderCanvas(context));
  }

  // Render the chart content by `canvasDrawer`.
  private renderCanvas(context: CanvasRenderingContext2D) {
    // TODO(b/350423216): Update the `scrollbarPosition`.
    const scrollbarPosition: number = 0;

    if (this.canvasDrawer === null) {
      return;
    }

    this.canvasDrawer.renderCanvas(
        context, this.$.mainCanvas.width, this.$.mainCanvas.height,
        scrollbarPosition, this.startTime, this.scale);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-line-chart': HealthdInternalsLineChartElement;
  }
}

customElements.define(
    HealthdInternalsLineChartElement.is, HealthdInternalsLineChartElement);
