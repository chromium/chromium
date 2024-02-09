// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SAMPLE_RATE} from './constants.js';

/**
 * Collect the data points to show on the line chart.
 * @const
 */
export class DataSeries {
  constructor(/** string */ title, /** string */ color) {
    /** @const {string} - The name of this data series. */
    this.title_ = title;

    /** @const {string} - The color of this data series. */
    this.color_ = color;

    /**
     * Whether the menu text color is black or not. To avoid showing white text
     * on light color.
     * @type {boolean}
     */
    this.isMenuTextBlack_ = false;

    /**
     * All the data points of the data series. Sorted by time.
     * @type {Array<{value: number, time: number}>}
     */
    this.dataPoints_ = [];

    /** @type {boolean} - To show or to hide the data series on the chart. */
    this.isVisible_ = true;

    /** @type {number} */
    this.cacheStartTime_ = 0;

    /** @type {number} */
    this.cacheStepSize_ = 0;

    /** @type {Array<number>} */
    this.cacheValues_ = [];

    /** @type {number} */
    this.cacheMaxValue_ = 0;
  }

  /**
   * Add a new data point to this data series. The time must be greater than the
   * time of the last data point in the data series.
   * @param {number} value
   * @param {number} time
   */
  addDataPoint(value, time) {
    if (!isFinite(value) || !isFinite(time)) {
      console.warn(
          `Add invalid value to DataSeries.Value: ${value} Time: ${time}`);
      return;
    }
    const /** number */ length = this.dataPoints_.length;
    if (length > 0 && this.dataPoints_[length - 1].time > time) {
      console.warn(
          'Add invalid time to DataSeries: ' + time +
          '. Time must be non-strictly increasing.');
      return;
    }
    const /** {value: number, time: number} */ point = {
      value: value,
      time: time,
    };
    this.dataPoints_.push(point);
    this.clearCacheValue_();
  }

  /** See |updateCacheValues_()| */
  clearCacheValue_() {
    this.cacheValues_ = [];
  }

  /**
   * Set the menu text is black or not. Default is false. Set it to true if the
   * color of the data series is too bright to show the white text.
   * @param {boolean} isTextBlack
   */
  setMenuTextBlack(isTextBlack) {
    this.isMenuTextBlack_ = isTextBlack;
  }

  /**
   * Control the visibility of data series.
   * @param {boolean} isVisible
   */
  setVisible(isVisible) {
    this.isVisible_ = isVisible;
  }

  /** @return {boolean} */
  isVisible() {
    return this.isVisible_;
  }

  /** @return {boolean} */
  isMenuTextBlack() {
    return this.isMenuTextBlack_;
  }

  /** @return {string} */
  getTitle() {
    return this.title_;
  }

  /** @return {string} */
  getColor() {
    return this.color_;
  }

  /**
   * Get the values to draw on screen.
   * @param {number} startTime - The time of first point.
   * @param {number} stepSize - The step size between two value points.
   * @param {number} count - The number of values.
   * @return {Array<number|null>} - If a cell of the array is null, it means
   *      that there is no any data point in this interval.
   *      See |getSampleValue_()|.
   */
  getValues(startTime, stepSize, count) {
    this.updateCacheValues_(startTime, stepSize, count);
    return this.cacheValues_;
  }

  /**
   * Get the maximum value in the query interval. See |getValues()|.
   * @param {number} startTime
   * @param {number} stepSize
   * @param {number} count
   * @return {number}
   */
  getMaxValue(startTime, stepSize, count) {
    this.updateCacheValues_(startTime, stepSize, count);
    return this.cacheMaxValue_;
  }

  /**
   * Implementation of querying the values and the max value. See |getValues()|.
   * @param {number} startTime
   * @param {number} stepSize
   * @param {number} count
   */
  updateCacheValues_(startTime, stepSize, count) {
    if (this.cacheStartTime_ === startTime &&
        this.cacheStepSize_ === stepSize &&
        this.cacheValues_.length === count) {
      return;
    }

    const /** Array<null|number> */ values = [];
    values.length = count;
    const /** number */ sampleRate = SAMPLE_RATE;
    let /** number */ endTime = startTime;
    const /** number */ firstIndex = this.findLowerBoundPointIndex_(startTime);
    let /** number */ nextIndex = firstIndex;
    let /** number */ maxValue = 0;

    for (let i = 0; i < count; ++i) {
      endTime += stepSize;
      const /** {value: (number|null), nextIndex: number} */ result =
          this.getSampleValue_(nextIndex, endTime);
      values[i] = result.value;
      nextIndex = result.nextIndex;
      maxValue = Math.max(maxValue, values[i]);
    }

    this.cacheValues_ = values;
    this.cacheStartTime_ = startTime;
    this.cacheStepSize_ = stepSize;
    this.cacheMaxValue_ = maxValue;

    this.backfillValuePoint_(0, firstIndex, startTime);
    const lastIndex = nextIndex;
    const lastPointStartTime = endTime - stepSize;
    this.backfillValuePoint_(count - 1, lastIndex, lastPointStartTime);
  }

  /**
   * Find the index of lower bound point by simple binary search.
   * @param {number} time
   * @return {number}
   */
  findLowerBoundPointIndex_(time) {
    let /** number */ lower = 0;
    let /** number */ upper = this.dataPoints_.length;
    while (lower < upper) {
      const /** number */ mid = Math.floor((lower + upper) / 2);
      if (time <= this.dataPoints_[mid].time) {
        upper = mid;
      } else {
        lower = mid + 1;
      }
    }
    return lower;
  }

  /**
   * Get a single sample value from |firstIndex| to |endTime|. Return the value
   * and the next index of the points.
   * If there are many data points in the query interval, return their average.
   * If there is no data point in the query interval, return null.
   * @param {number} firstIndex
   * @param {number} endTime
   * @return {{value: (number|null), nextIndex: number}}
   */
  getSampleValue_(firstIndex, endTime) {
    const /** Array<{value: number, time: number}> */ dataPoints =
        this.dataPoints_;
    let /** number */ nextIndex = firstIndex;
    let /** number */ currentValueSum = 0;
    let /** number */ currentValueNum = 0;
    while (nextIndex < dataPoints.length &&
           dataPoints[nextIndex].time < endTime) {
      currentValueSum += dataPoints[nextIndex].value;
      ++currentValueNum;
      ++nextIndex;
    }

    let /** number|null */ value = null;
    if (currentValueNum > 0) {
      value = currentValueSum / currentValueNum;
    }
    return {
      value: value,
      nextIndex: nextIndex,
    };
  }

  /**
   * Try to fill up a point by the liner interpolation. The points may be
   * sparse, and the line chart will be broken beside the edge of the chart. Try
   * to fillback the first and the last position to make the chart continuous.
   * @param {number} valueIndex - The index of the value we want to fillback.
   * @param {number} dataIndex - The index of the data point we need.
   * @param {number} boundaryTime - Time we want to fillback the value.
   */
  backfillValuePoint_(valueIndex, dataIndex, boundaryTime) {
    const dataPoints = this.dataPoints_;
    const values = this.cacheValues_;
    const maxValue = this.cacheMaxValue_;
    if (values[valueIndex] == null && dataIndex > 0 &&
        dataIndex < dataPoints.length) {
      values[valueIndex] = this.dataPointLinearInterpolation(
          dataPoints[dataIndex - 1], dataPoints[dataIndex], boundaryTime);
      this.cacheMaxValue_ = Math.max(this.cacheMaxValue_, values[valueIndex]);
    }
  }

  /**
   * Do the linear interpolation for the data points.
   * @param {{value: number, time: number}} pointA
   * @param {{value: number, time: number}} pointB
   * @param {number} position - The position we want to insert the new value.
   * @return {number}
   */
  dataPointLinearInterpolation(pointA, pointB, position) {
    return this.constructor.linearInterpolation(
        this.getTimeOnLattice(pointA.time), pointA.value,
        this.getTimeOnLattice(pointB.time), pointB.value,
        this.getTimeOnLattice(position));
  }

  /**
   * When drawing the points of the line chart, we don't really draw the point
   * at the real position. Instead, we split the time axis into multiple
   * interval, and draw them on the edge of the interval. This function can find
   * the edge time of a interval the original time fall in.
   * @param {number} time
   * @return {number}
   */
  getTimeOnLattice(time) {
    const startTime = this.cacheStartTime_;
    const stepSize = this.cacheStepSize_;
    return Math.floor((time - startTime) / stepSize) * stepSize;
  }

  /**
   * The linear interpolation implementation.
   * @param {number} x1
   * @param {number} y1
   * @param {number} x2
   * @param {number} y2
   * @param {number} x
   * @return {number}
   */
  static linearInterpolation(x1, y1, x2, y2, x) {
    if (x1 === x2) {
      return (y1 + y2) / 2;
    }
    const /** number */ ratio = (x - x1) / (x2 - x1);
    return (y2 - y1) * ratio + y1;
  }
}
