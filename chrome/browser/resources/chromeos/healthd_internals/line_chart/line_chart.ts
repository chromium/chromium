// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './menu.js';
import './scrollbar.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DEFAULT_TIME_SCALE, DRAG_RATE, MAX_TIME_SCALE, MIN_TIME_SCALE, MOUSE_WHEEL_SCROLL_RATE, MOUSE_WHEEL_UNITS, TOUCH_ZOOM_UNITS, ZOOM_RATE} from './configs.js';
import {getTemplate} from './line_chart.html.js';
import type {HealthdInternalsLineChartMenuElement} from './menu.js';
import type {HealthdInternalsLineChartScrollbarElement} from './scrollbar.js';
import {CanvasDrawer} from './utils/canvas_drawer.js';
import {DataSeries} from './utils/data_series.js';

/**
 * Return the distance of two touch points.
 */
function getTouchsDistance(touchA: Touch, touchB: Touch): number {
  const diffX: number = touchA.clientX - touchB.clientX;
  const diffY: number = touchA.clientY - touchB.clientY;
  return Math.sqrt(diffX * diffX + diffY * diffY);
}

export interface HealthdInternalsLineChartElement {
  $: {
    chartRoot: HTMLElement,
    mainCanvas: HTMLCanvasElement,
    chartMenu: HealthdInternalsLineChartMenuElement,
    chartScrollbar: HealthdInternalsLineChartScrollbarElement,
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

    this.initCanvasEventHandlers();

    const resizeObserver = new ResizeObserver(() => {
      this.resizeChart();
      this.updateChart();
    });
    resizeObserver.observe(this.$.chartRoot);

    window.addEventListener('bar-scroll', () => {
      this.updateChart();
    });

    window.addEventListener('menu-buttons-updated', () => {
      this.updateChart();
    });
  }

  // The start time of the time line chart. (Unix time)
  private startTime: number;
  // The end time of the time line chart. (Unix time)
  private endTime: number;

  // Horizontal scale of line chart. Number of milliseconds between two pixels.
  private timeScale: number = DEFAULT_TIME_SCALE;

  // The helper class to draw the canvas.
  private canvasDrawer: CanvasDrawer|null = null;

  // Used to avoid updating the graph multiple times in a single operation.
  private chartUpdateTimer: number = 0;

  // Status of dragging and touching events for scrolling and zooming.
  private isDragging: boolean = false;
  private dragX: number = 0;
  private isTouching: boolean = false;
  private touchX: number = 0;
  private touchZoomBase: number = 0;

  // Whether the line chart is visible. If not, we don't need to render canvas.
  private isVisible: boolean = false;

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

  // Update the end time of the line chart. Also update the line chart and
  // scrollbar to display latest data.
  updateEndTime(endTime: number) {
    if (this.canvasDrawer === null) {
      console.warn(
          'This `canvasDrawer` has not been initilaized yet. Call ',
          '`initCanvasDrawer()` before calling this function.');
      return;
    }
    this.endTime = endTime;
    this.updateScrollBar();
    this.updateChart();
  }

  // Update the start time of the line chart when removing data. Also update the
  // line chart and scrollbar to display latest data.
  updateStartTime(startTime: number) {
    this.startTime = Math.max(this.startTime, startTime);
    this.updateScrollBar();
    this.updateChart();
  }

  // Update the visibility of line chart. We don't need to render the chart when
  // the chart is not visible.
  updateVisibility(isVisible: boolean) {
    this.isVisible = isVisible;
    if (isVisible) {
      this.updateScrollBar();
      this.updateChart();
    }
  }

  // Overwrite the maximum value of the chart.
  setChartMaxValue(maxValue: number|null) {
    if (this.canvasDrawer === null) {
      console.warn(
          'This `canvasDrawer` has not been initilaized yet. Call ',
          '`initCanvasDrawer()` before calling this function.');
      return;
    }
    this.canvasDrawer.setFixedMaxValue(maxValue);
  }

  // Handle the wheeling, mouse dragging and touching events.
  private initCanvasEventHandlers() {
    const canvas: HTMLCanvasElement = this.$.mainCanvas;
    canvas.addEventListener('wheel', (e: Event) => this.onWheel(e));
    canvas.addEventListener('mousedown', (e: Event) => this.onMouseDown(e));
    canvas.addEventListener('mousemove', (e: Event) => this.onMouseMove(e));
    canvas.addEventListener('mouseup', (e: Event) => this.onMouseUpOrOut(e));
    canvas.addEventListener('mouseout', (e: Event) => this.onMouseUpOrOut(e));
    canvas.addEventListener('touchstart', (e: Event) => this.onTouchStart(e));
    canvas.addEventListener('touchmove', (e: Event) => this.onTouchMove(e));
    canvas.addEventListener('touchend', (e: Event) => this.onTouchEnd(e));
    canvas.addEventListener('touchcancel', (e: Event) => this.onTouchCancel(e));
  }

  // Mouse and touchpad scroll event. Horizontal scroll for chart scrolling,
  // vertical scroll for chart zooming.
  private onWheel(event: Event) {
    event.preventDefault();
    const wheelEvent: WheelEvent = event as WheelEvent;
    const wheelX: number = wheelEvent.deltaX / MOUSE_WHEEL_UNITS;
    const wheelY: number = -wheelEvent.deltaY / MOUSE_WHEEL_UNITS;
    this.scrollChart(MOUSE_WHEEL_SCROLL_RATE * wheelX);
    this.zoomChart(Math.pow(ZOOM_RATE, -wheelY));
  }

  // The following three functions handle mouse dragging event.
  private onMouseDown(event: Event) {
    event.preventDefault();
    this.isDragging = true;
    this.dragX = (event as MouseEvent).clientX;
  }

  private onMouseMove(event: Event) {
    event.preventDefault();
    if (!this.isDragging) {
      return;
    }
    const mouseEvent: MouseEvent = event as MouseEvent;
    const dragDeltaX = mouseEvent.clientX - this.dragX;
    this.scrollChart(DRAG_RATE * dragDeltaX);
    this.dragX = mouseEvent.clientX;
  }

  private onMouseUpOrOut(event: Event) {
    event.preventDefault();
    this.isDragging = false;
  }

  // The following four functions handle touch events. One finger for scrolling,
  // two finger for zooming.
  private onTouchStart(event: Event) {
    event.preventDefault();
    this.isTouching = true;
    const touches = (event as TouchEvent).targetTouches;
    if (touches.length === 1) {
      this.touchX = touches[0].clientX;
    } else if (touches.length === 2) {
      this.touchZoomBase = getTouchsDistance(touches[0], touches[1]);
    }
  }

  private onTouchMove(event: Event) {
    event.preventDefault();
    if (!this.isTouching) {
      return;
    }
    const touches: TouchList = (event as TouchEvent).targetTouches;
    if (touches.length === 1) {
      const dragDeltaX: number = this.touchX - touches[0].clientX;
      this.scrollChart(DRAG_RATE * dragDeltaX);
      this.touchX = touches[0].clientX;
    } else if (touches.length === 2) {
      const newDistance: number = getTouchsDistance(touches[0], touches[1]);
      const zoomDelta: number =
          (this.touchZoomBase - newDistance) / TOUCH_ZOOM_UNITS;
      this.zoomChart(Math.pow(ZOOM_RATE, zoomDelta));
      this.touchZoomBase = newDistance;
    }
  }

  private onTouchEnd(event: Event) {
    event.preventDefault();
    this.isTouching = false;
  }

  private onTouchCancel(event: Event) {
    event.preventDefault();
    this.isTouching = false;
  }

  // Zoom the line chart by setting the `timeScale` to `rate` times.
  private zoomChart(rate: number) {
    const oldScale: number = this.timeScale;
    const newScale: number = this.timeScale * rate;
    this.timeScale =
        Math.max(MIN_TIME_SCALE, Math.min(newScale, MAX_TIME_SCALE));

    if (this.timeScale === oldScale) {
      return;
    }

    if (this.$.chartScrollbar.isScrolledToRightEdge()) {
      this.updateScrollBar();
      this.$.chartScrollbar.scrollToRightEdge();
      this.updateChart();
      return;
    }

    // To try to make the chart keep right, make the right edge of the chart
    // stop at the same position.
    const oldPosition: number = this.$.chartScrollbar.getPosition();
    const canvasWidth: number = this.$.mainCanvas.width;
    const visibleEndTime: number = oldScale * (oldPosition + canvasWidth);
    const newPosition: number =
        Math.round(visibleEndTime / this.timeScale) - canvasWidth;

    this.updateScrollBar();
    this.$.chartScrollbar.setPosition(newPosition);
    this.updateChart();
  }

  // Scroll the line chart by moving forward `delta` pixels.
  private scrollChart(delta: number) {
    const oldPosition: number = this.$.chartScrollbar.getPosition();
    const newPosition: number = oldPosition + Math.round(delta);

    this.$.chartScrollbar.setPosition(newPosition);
    if (this.$.chartScrollbar.getPosition() === oldPosition) {
      return;
    }
    this.updateChart();
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
    this.$.chartScrollbar.resize(width);
    this.updateScrollBar();
  }

  // Update the scrollbar to the current line chart status after zooming,
  // scrolling... etc.
  private updateScrollBar() {
    if (!this.isVisible) {
      return;
    }
    const scrollRange: number =
        Math.max(this.getChartWidth() - this.getChartVisibleWidth(), 0);
    const isScrolledToRightEdge: boolean =
        this.$.chartScrollbar.isScrolledToRightEdge();
    this.$.chartScrollbar.setScrollableRange(scrollRange);
    if (isScrolledToRightEdge && !this.isDragging) {
      this.$.chartScrollbar.scrollToRightEdge();
    }
  }

  // Get the whole line chart width, in pixel.
  private getChartWidth(): number {
    const timeRange: number = this.endTime - this.startTime;
    return Math.floor(timeRange / this.timeScale);
  }

  // Get the visible chart width.
  private getChartVisibleWidth(): number {
    return this.$.chartRoot.offsetWidth - this.$.chartMenu.getWidth();
  }

  // Get the visible chart height.
  private getChartVisibleHeight(): number {
    return this.$.chartRoot.offsetHeight - this.$.chartScrollbar.getHeight();
  }

  // Render the line chart. Note that to avoid calling render function
  // multiple times in a single operation, this function will set a timeout
  // rather than calling render function directly.
  private updateChart() {
    clearTimeout(this.chartUpdateTimer);
    const context: CanvasRenderingContext2D|null =
        this.$.mainCanvas.getContext('2d');
    if (context === null || this.canvasDrawer === null ||
        !this.canvasDrawer.shouldRender() || !this.isVisible) {
      return;
    }
    this.chartUpdateTimer = setTimeout(() => this.renderCanvas(context));
  }

  // Render the chart content by `canvasDrawer`.
  private renderCanvas(context: CanvasRenderingContext2D) {
    // To reduce CPU usage, the chart do not draw points at every pixels.
    // We need to know the offset of data from `scrollbarPosition`.
    let scrollbarPosition: number = this.$.chartScrollbar.getPosition();
    if (this.$.chartScrollbar.getScrollableRange() === 0) {
      // If the chart width less than the visible width, make the chart align
      // right by setting the negative position.
      scrollbarPosition = this.getChartWidth() - this.$.mainCanvas.width;
    }

    if (this.canvasDrawer === null) {
      return;
    }

    const visibleStartTime: number =
        this.startTime + scrollbarPosition * this.timeScale;
    const visibleEndTime: number =
        visibleStartTime + this.getChartVisibleWidth() * this.timeScale;
    this.canvasDrawer.renderCanvas(
        context, this.$.mainCanvas.width, this.$.mainCanvas.height,
        visibleStartTime, visibleEndTime, this.timeScale);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-line-chart': HealthdInternalsLineChartElement;
  }
}

customElements.define(
    HealthdInternalsLineChartElement.is, HealthdInternalsLineChartElement);
