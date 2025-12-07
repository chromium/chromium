// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './menu.js';
import './scrollbar.js';
import './chart_summary_table.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LineChartController} from '../../controller/line_chart_controller.js';
import {CategoryTypeEnum} from '../../controller/system_trend_controller.js';
import type {DataSeriesList} from '../../controller/system_trend_controller.js';
import type {DataSeries} from '../../model/data_series.js';
import {DEFAULT_TIME_SCALE, DRAG_RATE, MAX_TIME_SCALE, MIN_TIME_SCALE, MOUSE_WHEEL_SCROLL_RATE, MOUSE_WHEEL_UNITS, TOUCH_ZOOM_UNITS, ZOOM_RATE} from '../../utils/line_chart_configs.js';

import type {HealthdInternalsChartSummaryTableElement} from './chart_summary_table.js';
import {getTemplate} from './line_chart.html.js';
import type {HealthdInternalsLineChartMenuElement} from './menu.js';
import type {HealthdInternalsLineChartScrollbarElement} from './scrollbar.js';

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
    summaryTable: HealthdInternalsChartSummaryTableElement,
    chartRoot: HTMLElement,
    mainCanvas: HTMLCanvasElement,
    chartMenu: HealthdInternalsLineChartMenuElement,
    chartScrollbar: HealthdInternalsLineChartScrollbarElement,
    chartContainer: HTMLElement,
  };
}

/**
 * Create a line chart element, including a left button menu, a bottom scrollbar
 * and a canvas. This element will enroll events of the line chart, handle
 * scroll, touch and resize events, and update canvas.
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

    this.initCanvasEventHandlers();

    const resizeObserver = new ResizeObserver(() => {
      this.resizeCanvas();
      this.updateCanvas();
    });
    resizeObserver.observe(this.$.chartRoot);

    window.addEventListener('bar-scroll', () => {
      this.updateCanvas();
    });

    window.addEventListener('menu-buttons-updated', () => {
      this.updateCanvas();
    });
  }

  // Controller for this UI element.
  private controller: LineChartController = new LineChartController(this);

  // Horizontal scale of line chart. Number of milliseconds between two pixels.
  private timeScale: number = DEFAULT_TIME_SCALE;

  // Status of dragging and touching events for scrolling and zooming.
  private isDragging: boolean = false;
  private dragX: number = 0;
  private isTouching: boolean = false;
  private touchX: number = 0;
  private touchZoomBase: number = 0;

  // Whether the line chart is visible. If not, we don't need to render canvas.
  private isVisible: boolean = false;

  // The start and end time of the visible part of line chart. Used to share
  // with parnet element.
  private visibleStartTime: number = 0;
  private visibleEndTime: number = 0;

  getController(): LineChartController {
    return this.controller;
  }

  getSummaryTable(): HealthdInternalsChartSummaryTableElement {
    return this.$.summaryTable;
  }

  /**
   * Sets up the data source for controller, menu and summary table.
   *
   * @param category - The current displayed category.
   * @param dataSeriesLists - List of `DataSeriesList` objects, which is used to
   *                          store data from different source. The Data shared
   *                          with the same scale with be stored into one
   *                          `DataSeriesList`.
   */
  setupDataSeries(
      category: CategoryTypeEnum, dataSeriesLists: DataSeriesList[]) {
    this.controller.setupDataSeries(category, dataSeriesLists);

    const flatDataList = dataSeriesLists.reduce(
        (acc, val) => acc.concat(val.dataList), [] as DataSeries[]);
    const isCustomCategory = category === CategoryTypeEnum.CUSTOM;
    this.$.chartMenu.setupDataSeries(flatDataList, isCustomCategory);
    this.$.summaryTable.setIsCustomCategory(isCustomCategory);

    this.resizeCanvas();
  }

  /**
   * Uses the latest data in controller to refresh the line chart content,
   * including scrollbar and canvas.
   */
  refreshLineChart() {
    this.controller.updateDataTime();
    this.updateScrollBar();
    this.updateCanvas();
  }

  /**
   * Updates the visibility of line chart. We don't need to render the chart
   * when the chart is not visible.
   */
  updateVisibility(isVisible: boolean) {
    this.isVisible = isVisible;
    if (isVisible) {
      this.refreshLineChart();
    }
  }

  /**
   * Updates the visibility for the chart summary table.
   */
  toggleChartSummaryTable(isVisible: boolean) {
    this.$.chartContainer.style.setProperty(
        '--summary-table-height', isVisible ? '200px' : '0px');
  }

  /**
   * Updates the visible time span and emits a custom event to notify the
   * `HealthdInternalsSystemTrendElement` component to update displayed info.
   *
   * @param visibleStartTime The new start time.
   * @param visibleEndTime The new end time.
   */
  updateVisibleTimeSpan(visibleStartTime: number, visibleEndTime: number) {
    this.visibleStartTime = visibleStartTime;
    this.visibleEndTime = visibleEndTime;
    this.dispatchEvent(
        new CustomEvent('time-range-changed', {bubbles: true, composed: true}));
  }

  /**
   * Returns the current visible time span as a tuple [startTime, endTime].
   */
  getVisibleTimeSpan(): [number, number] {
    return [this.visibleStartTime, this.visibleEndTime];
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

    this.updateScrollBar();
    if (this.$.chartScrollbar.isScrolledToRightEdge()) {
      this.$.chartScrollbar.scrollToRightEdge();
    } else {
      // To try to make the chart keep right, make the right edge of the chart
      // stop at the same position.
      const oldPosition: number = this.$.chartScrollbar.getPosition();
      const canvasWidth: number = this.$.mainCanvas.width;
      const visibleEndTime: number = oldScale * (oldPosition + canvasWidth);
      const newPosition: number =
          Math.round(visibleEndTime / this.timeScale) - canvasWidth;
      this.$.chartScrollbar.setPosition(newPosition);
    }
    this.updateCanvas();
  }

  // Scroll the line chart by moving forward `delta` pixels.
  private scrollChart(delta: number) {
    const oldPosition: number = this.$.chartScrollbar.getPosition();
    const newPosition: number = oldPosition + Math.round(delta);

    this.$.chartScrollbar.setPosition(newPosition);
    if (this.$.chartScrollbar.getPosition() === oldPosition) {
      return;
    }
    this.updateCanvas();
  }

  // Canvas requires explicit resizing instead of relying on CSS auto-rendering.
  private resizeCanvas() {
    const expectedWidth =
        this.$.chartRoot.offsetWidth - this.$.chartMenu.getWidth();
    const expectedHeight =
        this.$.chartRoot.offsetHeight - this.$.chartScrollbar.getHeight();
    if (this.$.mainCanvas.width === expectedWidth &&
        this.$.mainCanvas.height === expectedHeight) {
      return;
    }

    this.$.mainCanvas.width = expectedWidth;
    this.$.mainCanvas.height = expectedHeight;
    this.$.chartScrollbar.resize(expectedWidth);
    this.updateScrollBar();
  }

  // Update the scrollbar to the current line chart status after zooming,
  // scrolling, and resizing.
  private updateScrollBar() {
    if (!this.isVisible) {
      return;
    }

    const scrollbar = this.$.chartScrollbar;
    const range = this.controller.getScrollableRange(
        this.$.mainCanvas.width, this.timeScale);
    scrollbar.setScrollableRange(range);

    const isScrolledToRightEdge = scrollbar.isScrolledToRightEdge();
    if (isScrolledToRightEdge && !this.isDragging) {
      scrollbar.scrollToRightEdge();
    }
  }

  // Render the latest content in canvas context.
  private updateCanvas() {
    const canvas = this.$.mainCanvas;
    const context: CanvasRenderingContext2D|null = canvas.getContext('2d');
    if (context === null || !this.isVisible) {
      return;
    }

    this.controller.updateCanvas(
        context, canvas.width, canvas.height, this.timeScale,
        this.$.chartScrollbar.getPosition());
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-line-chart': HealthdInternalsLineChartElement;
  }
}

customElements.define(
    HealthdInternalsLineChartElement.is, HealthdInternalsLineChartElement);
