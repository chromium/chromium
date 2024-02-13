// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {createElementWithClassName} from 'chrome://resources/ash/common/util.js';

import {BACKGROUND_COLOR, CHART_MARGIN, DEFAULT_SCALE, DRAG_RATE, GRID_COLOR, MAX_SCALE, MIN_LABEL_VERTICAL_SPACING, MIN_SCALE, MIN_TIME_LABEL_HORIZONTAL_SPACING, MOUSE_WHEEL_SCROLL_RATE, MOUSE_WHEEL_UNITS, SAMPLE_RATE, TEXT_COLOR, TIME_STEP_UNITS, TOUCH_ZOOM_UNITS, ZOOM_RATE} from './constants.js';
import {DataSeries} from './data_series.js';
import {Menu} from './menu.js';
import {Scrollbar} from './scrollbar.js';
import {SubChart} from './sub_chart.js';
import {UnitLabel} from './unit_label.js';

/**
 * Create a canvas line chart. The object will enroll the events of the line
 * chart, handle the scroll and the touch events, render the time label, create
 * and control other object. See README for usage.
 * @const
 */
export class LineChart {
  constructor() {
    /** @type {Element} */
    this.rootDiv_ = null;

    /**
     * The start time of the time line chart. (Unix time)
     * @const {number}
     */
    this.startTime_ = Date.now();
    /**
     * The end time of the time line chart. (Unix time)
     * @type {number}
     */
    this.endTime_ = this.startTime_ + 1;

    /**
     * The scale of the line chart. Milliseconds per pixel.
     * @type {number}
     */
    this.scale_ = DEFAULT_SCALE;

    /**
     * |subChart| is the chart that all data series in it shares the same unit
     * label. There are two |SubChart| in |LineChart|, one's label align left,
     * another's align right. See |SubChart|.
     * @type {Array<SubChart>}
     */
    this.subCharts_ = [null, null];

    /**
     * Use a timer to avoid updating the graph multiple times in a single
     * operation.
     * @type {number}
     */
    this.chartUpdateTimer_ = 0;

    /* Dragging events status and touching events status. */
    /** @type {boolean} */
    this.isDragging_ = false;

    /** @type {number} */
    this.dragX_ = 0;

    /** @type {boolean} */
    this.isTouching_ = false;

    /** @type {number} */
    this.touchX_ = 0;

    /** @type {number} */
    this.touchZoomBase_ = 0;

    /**
     * The menu to control the visibility of data series. See |Menu|.
     * @const {Menu}
     */
    this.menu_ = new Menu(this.onMenuUpdate_.bind(this));

    /** @const {Element} */
    this.canvas_ = createElementWithClassName('canvas', 'line-chart-canvas');

    /** @const {CSSStyleDeclaration} */
    this.canvasStyle_ = window.getComputedStyle(this.canvas_);

    /**
     * A dummy scrollbar to scroll the line chart and to show the current
     * visible position of the line chair.
     * @const {Scrollbar}
     */
    this.scrollbar_ = new Scrollbar(this.update.bind(this));
  }

  /**
   * Attach the root div of
   * @param {Element} rootDiv
   */
  attachRootDiv(rootDiv) {
    if (this.rootDiv_ != null) {
      return;
    }

    this.rootDiv_ = rootDiv;

    const /** Element */ menuDiv = this.menu_.getRootDiv();
    this.rootDiv_.appendChild(menuDiv);

    const /** Element */ chartOuterDiv =
        createElementWithClassName('div', 'line-chart-canvas-outer');
    this.initCanvas_();
    chartOuterDiv.appendChild(this.canvas_);
    const /** Element */ scrollBarDiv = this.scrollbar_.getRootDiv();
    chartOuterDiv.appendChild(scrollBarDiv);
    this.rootDiv_.appendChild(chartOuterDiv);

    window.addEventListener('resize', this.onResize_.bind(this));
    /* Initialize the graph size. */
    this.resize_();
  }

  initCanvas_() {
    /**
     * Passive event will disable |preventDefault()|, set passive to false to
     * make sure this feature is disabled. The default value of passive is true
     * on some Chromebooks. see https://crbug.com/761698.
     * @param {Element} element
     * @param {string} name
     * @param {function(Event): undefined} handler
     */
    const enrollNonPassiveEvent = function(element, name, handler) {
      element.addEventListener(name, handler, {passive: false});
    };
    enrollNonPassiveEvent(this.canvas_, 'wheel', this.onWheel_.bind(this));
    enrollNonPassiveEvent(
        this.canvas_, 'mousedown', this.onMouseDown_.bind(this));
    enrollNonPassiveEvent(
        this.canvas_, 'mousemove', this.onMouseMove_.bind(this));
    enrollNonPassiveEvent(
        this.canvas_, 'mouseup', this.onMouseUpOrOut_.bind(this));
    enrollNonPassiveEvent(
        this.canvas_, 'mouseout', this.onMouseUpOrOut_.bind(this));
    enrollNonPassiveEvent(
        this.canvas_, 'touchstart', this.onTouchStart_.bind(this));
    enrollNonPassiveEvent(
        this.canvas_, 'touchmove', this.onTouchMove_.bind(this));
    enrollNonPassiveEvent(
        this.canvas_, 'touchend', this.onTouchEnd_.bind(this));
    enrollNonPassiveEvent(
        this.canvas_, 'touchcancel', this.onTouchCancel_.bind(this));

    const /** string */ pxString = `${CHART_MARGIN}px`;
    const /** string */ marginString = `${pxString} ${pxString} 0 ${pxString}`;
    this.canvas_.style.margin = marginString;
  }

  /**
   * Mouse and touchpad scroll event. Horizontal scroll for chart scrolling,
   * vertical scroll for chart zooming.
   * @param {Event} event
   */
  onWheel_(event) {
    event.preventDefault();
    /* WheelEvent.deltaMode will never be set to anything else but
     * DOM_DELTA_PIXEL. See crbug.com/227454 */
    if (event.deltaMode !== WheelEvent.DOM_DELTA_PIXEL) {
      console.warn(
          'WheelEvent.deltaMode is not set to WheelEvent.DOM_DELTA_PIXEL.');
    }
    const wheelX = event.deltaX / MOUSE_WHEEL_UNITS;
    const wheelY = -event.deltaY / MOUSE_WHEEL_UNITS;
    this.scroll(MOUSE_WHEEL_SCROLL_RATE * wheelX);
    this.zoom(Math.pow(ZOOM_RATE, -wheelY));
  }

  /**
   * The following three functions handle mouse dragging event.
   * @param {Event} event
   */
  onMouseDown_(event) {
    event.preventDefault();
    this.isDragging_ = true;
    this.dragX_ = event.clientX;
  }

  /**
   * @param {Event} event
   */
  onMouseMove_(event) {
    event.preventDefault();
    if (!this.isDragging_) {
      return;
    }
    const /** number */ dragDeltaX = event.clientX - this.dragX_;
    this.scroll(DRAG_RATE * dragDeltaX);
    this.dragX_ = event.clientX;
  }

  /**
   * @param {Event} event
   */
  onMouseUpOrOut_(event) {
    this.isDragging_ = false;
  }

  /**
   * Return the distance of two touch points.
   * @param {Touch} touchA
   * @param {Touch} touchB
   * @return {number}
   */
  static touchDistance_(touchA, touchB) {
    const /** number */ diffX = touchA.clientX - touchB.clientX;
    const /** number */ diffY = touchA.clientY - touchB.clientY;
    return Math.sqrt(diffX * diffX + diffY * diffY);
  }

  /**
   * The following four functions handle touch events. One finger for
   * scrolling, two finger for zooming.
   * @param {Event} event
   */
  onTouchStart_(/** Event*/ event) {
    event.preventDefault();
    this.isTouching_ = true;
    const /** TouchList */ touches = event.targetTouches;
    if (touches.length === 1) {
      this.touchX_ = touches[0].clientX;
    } else if (touches.length === 2) {
      this.touchZoomBase_ =
          this.constructor.touchDistance_(touches[0], touches[1]);
    }
  }

  /**
   * @param {Event} event
   */
  onTouchMove_(event) {
    event.preventDefault();
    if (!this.isTouching_) {
      return;
    }
    const /** TouchList */ touches = event.targetTouches;
    if (touches.length === 1) {
      const /** number */ dragDeltaX = this.touchX_ - touches[0].clientX;
      this.scroll(DRAG_RATE * dragDeltaX);
      this.touchX_ = touches[0].clientX;
    } else if (touches.length === 2) {
      const /** number */ newDistance =
          this.constructor.touchDistance_(touches[0], touches[1]);
      const /** number */ zoomDelta =
          (this.touchZoomBase_ - newDistance) / TOUCH_ZOOM_UNITS;
      this.zoom(Math.pow(ZOOM_RATE, zoomDelta));
      this.touchZoomBase_ = newDistance;
    }
  }

  /**
   * @param {Event} event
   */
  onTouchEnd_(event) {
    event.preventDefault();
    this.isTouching_ = false;
  }

  /**
   * @param {Event} event
   */
  onTouchCancel_(event) {
    event.preventDefault();
    this.isTouching_ = false;
  }

  /**
   * Zoom the line chart by setting the |scale| to |rate| times.
   * @param {number} rate
   */
  zoom(rate) {
    const /** number */ oldScale = this.scale_;
    const /** number */ newScale = this.scale_ * rate;
    this.scale_ = Math.max(MIN_SCALE, Math.min(newScale, MAX_SCALE));

    if (this.scale_ === oldScale) {
      return;
    }

    if (this.scrollbar_.isScrolledToRightEdge()) {
      this.updateScrollBar_();
      this.scrollbar_.scrollToRightEdge();
      this.update();
      return;
    }

    /* To try to make the chart keep right, make the right edge of the chart
     * stop at the same position. */
    const /** number */ oldPosition = this.scrollbar_.getPosition();
    const /** number */ width = this.canvas_.width;
    const /** number */ visibleEndTime = oldScale * (oldPosition + width);
    const /** number */ newPosition =
        Math.round(visibleEndTime / this.scale_) - width;

    this.updateScrollBar_();
    this.scrollbar_.setPosition(newPosition);

    this.update();
  }

  /**
   * Scroll the line chart by moving forward |delta| pixels.
   * @param {number} delta
   */
  scroll(delta) {
    const /** number */ oldPosition = this.scrollbar_.getPosition();
    const /** number */ newPosition = oldPosition + Math.round(delta);

    this.scrollbar_.setPosition(newPosition);
    if (this.scrollbar_.getPosition() === oldPosition) {
      return;
    }

    this.update();
  }

  /**
   * Handle window resize event.
   */
  onResize_() {
    this.resize_();
    this.update();
  }

  /**
   * Handle |Menu| update event.
   */
  onMenuUpdate_() {
    this.resize_();
    this.update();
  }

  resize_() {
    const width = this.getChartVisibleWidth();
    const height = this.getChartVisibleHeight();
    if (this.canvas_.width === width && this.canvas_.height === height) {
      return;
    }

    this.canvas_.width = width;
    this.canvas_.height = height;
    const /** number */ scrollBarWidth = width + 2 * CHART_MARGIN;
    this.scrollbar_.resize(scrollBarWidth);
    this.updateScrollBar_();
  }

  /**
   * Update the end time of the line chart. This will update the scrollable
   * range of the chart. If the original position is at the right edge, the
   * chart will automatically scroll to right edge after updating.
   * @param {number} endTime
   */
  updateEndTime(endTime) {
    this.endTime_ = endTime;
    this.updateScrollBar_();
    this.update();
  }

  /**
   * Update the scrollbar to the current line chart status after zooming,
   * scrolling... etc.
   */
  updateScrollBar_() {
    const /** number */ scrollRange =
        Math.max(this.getChartWidth_() - this.getChartVisibleWidth(), 0);
    const /** boolean */ isScrolledToRightEdge =
        this.scrollbar_.isScrolledToRightEdge();
    this.scrollbar_.setRange(scrollRange);
    if (isScrolledToRightEdge && !this.isDragging_) {
      this.scrollbar_.scrollToRightEdge();
    }
  }

  /**
   * Get the whole line chart width, in pixel.
   * @return {number}
   */
  getChartWidth_() {
    const /** number */ timeRange = this.endTime_ - this.startTime_;

    const /** number */ numOfPixels = Math.floor(timeRange / this.scale_);
    const /** number */ sampleRate = SAMPLE_RATE;
    /* To reduce CPU usage, the chart do not draw points at every pixels.
     * Remove the last few pixels to avoid the graph showing some blank at
     * the end of the graph. */
    const /** number */ extraPixels = numOfPixels % sampleRate;
    return numOfPixels - extraPixels;
  }

  /**
   * Get the visible chart width, the width we need to render to the canvas, in
   * pixel.
   * @return {number}
   */
  getChartVisibleWidth() {
    return this.rootDiv_.offsetWidth - CHART_MARGIN * 2 - this.menu_.getWidth();
  }

  /**
   * Get the visible chart height.
   * @return {number}
   */
  getChartVisibleHeight() {
    return this.rootDiv_.offsetHeight - CHART_MARGIN -
        this.scrollbar_.getHeight();
  }

  /**
   * Set or reset the |units| and the |unitBase| of the |SubChart|.
   * @param {number} align - The align side of the subchart.
   * @param {Array<string>} units - See |UnitLabel|.
   * @param {number} unitBase - See |UnitLabel|.
   */
  setSubChart(align, units, unitBase) {
    this.clearSubChart(align);
    const /** UnitLabel */ label = new UnitLabel(units, unitBase);
    this.subCharts_[align] = new SubChart(label, align);
    this.update();
  }

  /**
   * Overwrite the maximum value of the sub chart. See |SubChart.setMaxValue|.
   * @param {number} align - The align side of the subchart.
   * @param {number|null} maxValue
   */
  setSubChartMaxValue(align, maxValue) {
    if (this.subCharts_[align]) {
      this.subCharts_[align].setMaxValue(maxValue);
    }
  }

  /**
   * Clear all subcharts and data series in the line chart.
   */
  clearAllSubChart() {
    for (let /** number */ i = 0; i < this.subCharts_.length; ++i) {
      this.clearSubChart(i);
    }
    this.update();
  }

  /**
   * Clear a single subchart and its data series.
   * @param {number} align - The align side of the subchart.
   */
  clearSubChart(align) {
    const /** SubChart */ oldSubChart = this.subCharts_[align];
    if (oldSubChart) {
      const /** Array<DataSeries> */ dataSeriesList =
          oldSubChart.getDataSeriesList();
      for (let /** number */ i = 0; i < dataSeriesList.length; ++i) {
        this.menu_.removeDataSeries(dataSeriesList[i]);
      }
    }
    this.subCharts_[align] = null;
    this.update();
  }

  /**
   * Add a data series to a subchart of the line chart. Call |setSubChart|
   * before calling this function.
   * @param {number} align - The align side of the subchart.
   * @param {DataSeries} dataSeries
   */
  addDataSeries(align, dataSeries) {
    const /** Array<SubChart> */ subCharts = this.subCharts_;
    if (subCharts[align] == null) {
      console.warn(
          'This sub chart has not been setup yet. ' +
          'Call |setSubChart| before calling this function.');
      return;
    }
    this.subCharts_[align].addDataSeries(dataSeries);
    this.menu_.addDataSeries(dataSeries);
    this.update();
  }

  /**
   * Render the line chart. Note that to avoid calling render function
   * multiple times in a single operation, this function will set a timeout
   * rather than calling render function directly.
   */
  update() {
    clearTimeout(this.chartUpdateTimer_);
    if (!this.shouldRender()) {
      return;
    }
    this.chartUpdateTimer_ = setTimeout(this.render_.bind(this));
  }

  /**
   * Return true if any subchart need to be rendered.
   * @return {boolean}
   */
  shouldRender() {
    const subCharts = this.subCharts_;
    for (let /** number */ i = 0; i < subCharts.length; ++i) {
      if (subCharts[i] != null && subCharts[i].shouldRender()) {
        return true;
      }
    }
    return false;
  }

  /**
   * Implementation of line chart rendering.
   */
  render_() {
    const /** CanvasRenderingContext2D */ context =
        this.canvas_.getContext('2d');
    const /** number */ width = this.canvas_.width;
    const /** number */ height = this.canvas_.height;

    this.initAndClearContext_(context, width, height);

    const /** number */ fontHeight = parseInt(this.canvasStyle_.fontSize, 10);
    if (!fontHeight) {
      console.warn(
          'Render failed. Cannot get the font height from the canvas ' +
          'font style string.');
      return;
    }

    let /** number */ position = this.scrollbar_.getPosition();
    if (this.scrollbar_.getRange() === 0) {
      /* If the chart width less than the visible width, make the chart align
       * right by setting the negative position. */
      position = this.getChartWidth_() - this.canvas_.width;
    }
    const /** number */ visibleStartTime =
        this.startTime_ + position * this.scale_;
    const /** number */ graphHeight =
        height - fontHeight - MIN_LABEL_VERTICAL_SPACING;
    this.renderTimeLabels_(
        context, width, graphHeight, fontHeight, visibleStartTime);
    this.renderSubCharts_(
        context, width, graphHeight, fontHeight, visibleStartTime, position);
    this.renderChartGrid_(context, width, graphHeight);
  }

  /**
   * @param {CanvasRenderingContext2D} context
   * @param {number} width
   * @param {number} height
   */
  initAndClearContext_(context, width, height) {
    context.font = this.canvasStyle_.getPropertyValue('font');
    context.lineWidth = 2;
    context.lineCap = 'round';
    context.lineJoin = 'round';
    context.fillStyle = BACKGROUND_COLOR;
    context.fillRect(0, 0, width, height);
  }

  /**
   * Render the time label under the line chart.
   * @param {CanvasRenderingContext2D} context
   * @param {number} width
   * @param {number} height
   * @param {number} fontHeight
   * @param {number} startTime - The start time of the time label.
   */
  renderTimeLabels_(context, width, height, fontHeight, startTime) {
    const /** string */ sampleText = (new Date(startTime)).toLocaleTimeString();
    const /** number */ minSpacing = context.measureText(sampleText).width +
        MIN_TIME_LABEL_HORIZONTAL_SPACING;
    const /** number */ timeStep =
        this.constructor.getSuitableTimeStep_(minSpacing, this.scale_);
    if (timeStep === 0) {
      console.warn('Render time label failed. Cannot find suitable time unit.');
      return;
    }

    context.textBaseline = 'bottom';
    context.textAlign = 'center';
    context.fillStyle = TEXT_COLOR;
    context.strokeStyle = GRID_COLOR;
    context.beginPath();
    const /** number */ yCoord =
        height + fontHeight + MIN_LABEL_VERTICAL_SPACING;
    const /** number */ firstTimeTick =
        Math.ceil(startTime / timeStep) * timeStep;
    let /** number */ time = firstTimeTick;
    while (true) {
      const /** number */ xCoord = Math.round((time - startTime) / this.scale_);
      if (xCoord >= width) {
        break;
      }
      const /** string */ text = (new Date(time)).toLocaleTimeString();
      context.fillText(text, xCoord, yCoord);
      context.moveTo(xCoord, 0);
      context.lineTo(xCoord, height - 1);
      time += timeStep;
    }
    context.stroke();
  }

  /**
   * Find the suitable step of time to render the time label.
   * @param {number} minSpacing - The minimum spacing between two time tick.
   * @param {number} scale - The scale of the line chart.
   * @return {number}
   */
  static getSuitableTimeStep_(minSpacing, scale) {
    const /** Array<number> */ timeStepUnits = TIME_STEP_UNITS;
    let /** number */ timeStep = 0;
    for (let /** number */ i = 0; i < timeStepUnits.length; ++i) {
      if (timeStepUnits[i] / scale >= minSpacing) {
        timeStep = timeStepUnits[i];
        break;
      }
    }
    return timeStep;
  }

  /**
   * @param {CanvasRenderingContext2D} context
   * @param {number} graphWidth
   * @param {number} graphHeight
   */
  renderChartGrid_(context, graphWidth, graphHeight) {
    context.strokeStyle = GRID_COLOR;
    context.strokeRect(0, 0, graphWidth - 1, graphHeight);
  }

  /**
   * Render the subcharts and their data series.
   * @param {CanvasRenderingContext2D} context
   * @param {number} graphWidth
   * @param {number} graphHeight
   * @param {number} fontHeight
   * @param {number} visibleStartTime
   * @param {number} position - The scrollbar position.
   */
  renderSubCharts_(
      context, graphWidth, graphHeight, fontHeight, visibleStartTime,
      position) {
    const /** Array<SubChart> */ subCharts = this.subCharts_;
    /* To reduce CPU usage, the chart do not draw points at every pixels. Use
     * |offset| to make sure the graph won't shaking during scrolling, the line
     * chart will render the data points at the same absolute position. */
    const /** number */ offset = position % SAMPLE_RATE;
    for (let /** number */ i = 0; i < subCharts.length; ++i) {
      if (subCharts[i] === undefined) {
        continue;
      }
      subCharts[i].setLayout(
          graphWidth, graphHeight, fontHeight, visibleStartTime, this.scale_,
          offset);
      subCharts[i].renderLines(context);
      subCharts[i].renderUnitLabels(context);
    }
  }
}
