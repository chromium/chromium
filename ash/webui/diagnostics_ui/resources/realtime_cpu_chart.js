// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/d3/d3.min.js';
import './diagnostics_shared.css.js';
import './strings.m.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './realtime_cpu_chart.html.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const RealtimeCpuChartElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/**
 * @fileoverview
 * 'realtime-cpu-chart' is a moving stacked area graph component used to display
 * a realtime cpu usage information.
 */

/** @polymer */
export class RealtimeCpuChartElement extends RealtimeCpuChartElementBase {
  static get is() {
    return 'realtime-cpu-chart';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** @type {number} */
      user: {
        type: Number,
        value: 0,
      },

      /** @type {number} */
      system: {
        type: Number,
        value: 0,
      },

      /** @private {number} */
      numDataPoints_: {
        type: Number,
        value: 50,
      },

      /**
       * @private {number}
       */
      dataRefreshPerSecond_: {
        type: Number,
        value: 2,
      },

      /**
       * Chart rendering frames per second.
       * Strongly preferred to be multiples of dataRefreshPerSecond_. If not,
       * there will be a small (hard to notice) jittering at every data refresh.
       * @private {number}
       */
      framesPerSecond_: {
        type: Number,
        value: 30,
      },

      /**
       * Duration of each frame in milliseconds
       * @private {number}
       */
      frameDuration_: {
        readOnly: true,
        type: Number,
        computed: 'getFrameDuration_(dataRefreshPerSecond_, framesPerSecond_)',
      },

      /** @private {number} */
      width_: {
        type: Number,
        value: 560,
      },

      /** @private {number} */
      height_: {
        type: Number,
        value: 114,
      },

      /** @private {!Object} */
      padding_: {
        type: Object,
        value: {top: 10, right: 5, bottom: 8, left: 50, tick: 10},
      },

      /** @private {number} */
      graphWidth_: {
        readOnly: true,
        type: Number,
        computed: 'getGraphDimension_(width_, padding_.left, padding_.right)',
      },

      /** @private {number} */
      graphHeight_: {
        readOnly: true,
        type: Number,
        computed: 'getGraphDimension_(height_, padding_.top, padding_.bottom)',
      },

    };
  }

  /** @override */
  constructor() {
    super();
    /**
     * Helper function to map range of x coordinates to graph width.
     * @private {?d3.LinearScale}
     */
    this.xAxisScaleFn_ = null;

    /**
     * Helper function to map range of y coordinates to
     * graph height.
     * @private {?d3.LinearScale}
     */
    this.yAxisScaleFn_ = null;

    /** @private {!Array<!Object>} */
    this.data_ = [];

    /**
     * DOMHighResTimeStamp of last graph render.
     * @private {number}
     */
    this.lastRender_ = 0;

    /**
     * Current render frame out of this.framesPerSecond_.
     * @private {number}
     */
    this.currentFrame_ = 0;

    /**
     * Y-Values where we should mark ticks for the y-axis on the left.
     * @private {!Array<number>}
     */
    this.yAxisTicks_ = [0, 25, 50, 75, 100];

    // Initialize the data array with data outside the chart boundary.
    // Note that with side nav DOM manipulation, created() isn't guaranteed to
    // be called only once.
    this.data_ = [];
    for (let i = 0; i < this.numDataPoints_; ++i) {
      this.data_.push({user: -1, system: -1});
    }
  }

  static get observers() {
    return ['setScaling_(graphWidth_)'];
  }

  /** @override */
  ready() {
    super.ready();
    this.setScaling_();
    this.initializeChart_();
    window.addEventListener('resize', () => this.updateChartWidth_());

    // Set the initial chart width.
    this.updateChartWidth_();
  }

  /** @private */
  updateChartWidth_() {
    // parseFloat() is used to convert the string returned by
    // getComputedStyleValue() into a number ("642px" --> 642).
    this.width_ = parseFloat(
        window.getComputedStyle(this).getPropertyValue('--chart-width-nav'));
  }

  /**
   * Calculate the duration of each frame in milliseconds.
   * @return {number}
   * @protected
   */
  getFrameDuration_() {
    assert(this.dataRefreshPerSecond_ > 0);
    assert(this.framesPerSecond_ > 0);
    assert(this.framesPerSecond_ % this.dataRefreshPerSecond_ === 0);
    return 1000 / (this.framesPerSecond_ / this.dataRefreshPerSecond_);
  }

  /**
   * Get actual graph dimensions after accounting for margins.
   * @param {number} base value of dimension.
   * @param {...number} margins related to base dimension.
   * @return {number}
   * @private
   */
  getGraphDimension_(base, ...margins) {
    return margins.reduce(((acc, margin) => acc - margin), base);
  }

  /**
   * Sets scaling functions that convert data -> svg coordinates.
   * @private
   */
  setScaling_() {
    // Map y-values [0, 100] to [graphHeight, 0] inverse linearly.
    // Data value of 0 will map to graphHeight, 100 maps to 0.
    this.yAxisScaleFn_ =
        d3.scaleLinear().domain([0, 100]).range([this.graphHeight_, 0]);

    // Map x-values [0, numDataPoints - 3] to [0, graphWidth] linearly.
    // Data value of 0 maps to 0, and (numDataPoints - 2) maps to graphWidth.
    // numDataPoints is subtracted since 1) data array is zero based, and
    // 2) to smooth out the curve function.
    this.xAxisScaleFn_ =
        d3.scaleLinear().domain([0, this.numDataPoints_ - 2]).range([
          0,
          this.graphWidth_,
        ]);

    // Draw the y-axis legend and also draw the horizontal gridlines by
    // reversing the ticks back into the chart body.
    const chartGroup = d3.select(this.shadowRoot.querySelector('#chartGroup'));
    chartGroup.select('#gridLines')
        .call(
            d3.axisLeft(/** @type {!d3.LinearScale} */ (this.yAxisScaleFn_))
                .tickValues(this.yAxisTicks_)
                .tickFormat((y) => this.getPercentageLabel_(y))
                .tickPadding(this.padding_.tick)
                .tickSize(-this.graphWidth_),  // Extend the ticks into the
                                               // entire graph as gridlines.
        );
  }

  /** @private */
  initializeChart_() {
    const chartGroup = d3.select(this.shadowRoot.querySelector('#chartGroup'));

    // Position chartGroup inside the margin.
    chartGroup.attr(
        'transform',
        'translate(' + this.padding_.left + ',' + this.padding_.top + ')');

    const plotGroup = d3.select(this.shadowRoot.querySelector('#plotGroup'));

    // Feed data array to the plot group.
    plotGroup.datum(this.data_);

    // Select each area and configure the transition for animation.
    // d3.transition API @ https://github.com/d3/d3-transition#d3-transition.
    // d3.easing API @ https://github.com/d3/d3-ease#api-reference.
    plotGroup.select('.user-area')
        .transition()
        .duration(this.frameDuration_)
        .ease(d3.easeLinear);  // Linear transition
    plotGroup.select('.system-area')
        .transition()
        .duration(this.frameDuration_)
        .ease(d3.easeLinear);  // Linear transition

    // Draw initial data and kick off the rendering process.
    this.getDataSnapshotAndRedraw_();
    this.render_(0);
  }

  /**
   * @param {string} areaClass class string for <path> element.
   * @return {d3.Area}
   * @private
   */
  getAreaDefinition_(areaClass) {
    return d3
        .area()
        // Take the index of each data as x values.
        .x((data, i) => this.xAxisScaleFn_(i))
        // Bottom coordinates of each area. System area extends down to -1
        // instead of 0 to avoid the area border from showing up.
        .y0(data => this.yAxisScaleFn_(
                areaClass === 'system-area' ? -1 : data.system))
        // Top coordinates of each area.
        .y1(data => this.yAxisScaleFn_(
                areaClass === 'system-area' ? data.system :
                                              data.system + data.user));
  }

  /**
   * Takes a snapshot of current CPU readings and appends to the data array for
   * redrawing. This method is called after each transition cycle.
   * @private
   */
  getDataSnapshotAndRedraw_() {
    this.data_.push({user: this.user, system: this.system});
    this.data_.shift();

    const userArea = assert(this.shadowRoot.querySelector(`path.user-area`));
    const systemArea =
        assert(this.shadowRoot.querySelector(`path.system-area`));
    d3.select(userArea).attr('d', this.getAreaDefinition_('user-area'));
    d3.select(systemArea).attr('d', this.getAreaDefinition_('system-area'));
  }

  /**
   * @param {number} timeStamp Current time based on DOMHighResTimeStamp.
   * @private
   */
  render_(timeStamp) {
    // Re-render only when this.frameDuration_ has passed since last render.
    // If we acquire the animation frame before this, do nothing.
    if (timeStamp - this.lastRender_ > this.frameDuration_) {
      this.lastRender_ = performance.now();

      // Get new data and redraw on each cycle.
      const framesPerCycle = this.framesPerSecond_ / this.dataRefreshPerSecond_;
      if (this.currentFrame_ === framesPerCycle) {
        this.currentFrame_ = 0;
        this.getDataSnapshotAndRedraw_();
      }

      const userArea = assert(this.shadowRoot.querySelector(`path.user-area`));
      const systemArea =
          assert(this.shadowRoot.querySelector(`path.system-area`));

      // Calculate the new position. Use this.currentFrame_ + 1 since on frame
      // 0, it is already at position 0.
      const pos = -1 * ((this.currentFrame_ + 1) / framesPerCycle);

      // Slide it to the left
      d3.select(userArea).attr(
          'transform', 'translate(' + this.xAxisScaleFn_(pos) + ',0)');
      d3.select(systemArea)
          .attr('transform', 'translate(' + this.xAxisScaleFn_(pos) + ',0)');
      this.currentFrame_++;
    }

    // Request another frame.
    requestAnimationFrame((timeStamp) => this.render_(timeStamp));
  }

  /**
   * @param {number} value of percentage.
   * @return {string} i18n string for the percentage value.
   * @private
   */
  getPercentageLabel_(value) {
    return loadTimeData.getStringF('percentageLabel', value);
  }
}

customElements.define(RealtimeCpuChartElement.is, RealtimeCpuChartElement);
