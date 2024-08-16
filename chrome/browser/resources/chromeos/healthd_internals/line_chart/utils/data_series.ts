// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

/**
 * The interface of data points displayed in the line chart.
 */
export interface DataPoint {
  value: number;
  time: number;
}

/**
 * Get the average point from a range of points.
 */
function getAveragePoint(points: DataPoint[]): DataPoint {
  const valueSum: number = points.reduce((acc, item) => acc + item.value, 0);
  const timeSum: number = points.reduce((acc, item) => acc + item.time, 0);
  return {value: valueSum / points.length, time: timeSum / points.length};
}

/**
 * The helper class to collect data points, to record title and color and to get
 * the required values for displaying on the line chart.
 */
export class DataSeries {
  constructor(title: string, color: string) {
    this.displayedTitle = title;
    this.displayedColor = color;
  }

  // The name of this data series.
  private readonly displayedTitle: string;
  // The color of this data series.
  private readonly displayedColor: string;

  // All the data points of the data series. Sorted by time.
  private dataPoints: DataPoint[] = [];

  // Whether the data is visible on the line chart.
  private isVisible: boolean = true;

  // We will report the displayed points based on the cached data below. If the
  // cached data is the same, we don't need to calculate the points again.
  private cachedStartTime: number = 0;
  private cachedEndTime: number = 0;
  private cachedStepSize: number = 0;

  // Used to display partial line chart on the canvas.
  private displayedPoints: DataPoint[] = [];

  // The maximum value of `displayedPoints` value.
  private maxValue: number = 0;

  // Add a new data point to this data series. The time must be greater than the
  // time of the last data point in the data series.
  addDataPoint(value: number, time: number) {
    if (!isFinite(value) || !isFinite(time)) {
      console.warn(
          'Add invalid data to DataSeries: value: ' + value +
          ', time: ' + time);
      return;
    }
    const length: number = this.dataPoints.length;
    if (length > 0 && this.dataPoints[length - 1].time > time) {
      console.warn(
          'Add invalid time to DataSeries: ' + time +
          '. Time must be non-strictly increasing.');
      return;
    }
    this.dataPoints.push({value: value, time: time});
  }

  // Control the visibility of data series.
  setVisible(isVisible: boolean) {
    this.isVisible = isVisible;
  }

  getVisible(): boolean {
    return this.isVisible;
  }

  getTitle(): string {
    return this.displayedTitle;
  }

  getColor(): string {
    return this.displayedColor;
  }

  /**
   * Get the displayed points to draw on line chart.
   *
   * @param startTime - The start time for the displayed part of chart.
   * @param endTime - The end time for the displayed part of chart.
   * @param stepSize - The step size in millisecond. We will only display one
   *                   point in one step.
   * @return - The displayed points.
   */
  getDisplayedPoints(startTime: number, endTime: number, stepSize: number):
      DataPoint[] {
    if (!this.isVisible) {
      return [];
    }
    this.updateCachedData(startTime, endTime, stepSize);
    return this.displayedPoints;
  }

  // Get the maximum value of the displayed points. See `getDisplayedPoints()`
  // for details of arguments.
  getDisplayedMaxValue(startTime: number, endTime: number, stepSize: number):
      number {
    if (!this.isVisible) {
      return 0;
    }
    this.updateCachedData(startTime, endTime, stepSize);
    return this.maxValue;
  }

  // Implementation of querying the displayed points and the max value.
  private updateCachedData(
      startTime: number, endTime: number, stepSize: number) {
    if (this.cachedStartTime === startTime && this.cachedEndTime === endTime &&
        this.cachedStepSize === stepSize) {
      return;
    }

    this.displayedPoints =
        this.getDisplayedPointsInner(startTime, endTime, stepSize);
    this.maxValue = this.displayedPoints.reduce(
        (maxValue, item) => Math.max(maxValue, item.value), 0);

    // Updated the cached value after the data is updated.
    this.cachedStartTime = startTime;
    this.cachedEndTime = endTime;
    this.cachedStepSize = stepSize;
  }

  private getDisplayedPointsInner(
      startTime: number, endTime: number, stepSize: number): DataPoint[] {
    const output: DataPoint[] = [];
    let pointsInStep: DataPoint[] = [];
    // Helper function to collect one point in the current step.
    const storeDisplayedPoint = () => {
      if (pointsInStep.length !== 0) {
        output.push(getAveragePoint(pointsInStep));
        pointsInStep = [];
      }
    };

    // To avoid shaking, we need to minus the offset (`startTime % stepSize`) to
    // keep every step the same under the same `stepSize`.
    // Also minus `stepSize` to collect one more point before `startTime` to
    // avoid showing blank at the start.
    let currentStepStart: number = startTime - startTime % stepSize - stepSize;
    let currentIndex: number = this.findLowerBoundPointIndex(currentStepStart);

    // Return empty point list as there is no visible point after `startTime`.
    if (currentIndex >= this.dataPoints.length) {
      return [];
    }

    // If there is no point in the step before `startTime`, we will collect one
    // more point before `currentIndex`.
    if (this.dataPoints[currentIndex].time > startTime && currentIndex >= 1) {
      output.push(this.dataPoints[currentIndex - 1]);
    }

    while (currentIndex < this.dataPoints.length) {
      // Collect one more point outside the visible time to avoid showing blank
      // at the end.
      if (output.length !== 0 && output[output.length - 1].time >= endTime) {
        break;
      }

      const currentPoint: DataPoint = this.dataPoints[currentIndex];
      // Time of current point should be greater than or equal to
      // `currentStepStart`. See `findLowerBoundPointIndex()` for more details.
      assert(currentPoint.time >= currentStepStart);

      // Collect the points in [`currentStepStart`, `currentStepEnd`).
      const currentStepEnd: number = currentStepStart + stepSize;
      if (currentPoint.time >= currentStepEnd) {
        storeDisplayedPoint();
        currentStepStart = currentStepEnd;
      } else {
        pointsInStep.push(currentPoint);
        currentIndex += 1;
      }
    }
    storeDisplayedPoint();
    return output;
  }

  /**
   * Find the minimum index of point which time is greater than or equal to
   * `time` by simple binary search.
   */
  private findLowerBoundPointIndex(time: number): number {
    let lower: number = 0;
    let upper: number = this.dataPoints.length;
    while (lower < upper) {
      const mid: number = Math.floor((lower + upper) / 2);
      const point: DataPoint = this.dataPoints[mid];
      if (time <= point.time) {
        upper = mid;
      } else {
        lower = mid + 1;
      }
    }
    return lower;
  }

  /**
   * Filter out data points which the `time` field is earlier than `startTime`.
   * Keep an additional buffer of data points to avoid modifying `dataPoints`
   * too frequently.
   *
   * @param startTime - The new start time. Data points which time before this
   *                    should be removed.
   * @returns - Whether any data points are removed.
   */
  removeOutdatedData(startTime: number): boolean {
    // Retain one hour more of data points as buffer so we only need to update
    // `dataPoints` every hour.
    const dataRetentionBuffer: number = 1 * 60 * 60 * 1000;

    // Find the index of the first data point within the buffer.
    const bufferStartIndex: number =
        this.findLowerBoundPointIndex(startTime - dataRetentionBuffer);

    // If there are points outside the buffer, remove them and points in buffer.
    if (bufferStartIndex > 0) {
      const newStartIndex: number = this.findLowerBoundPointIndex(startTime);
      assert(newStartIndex >= bufferStartIndex);
      this.dataPoints.splice(0, newStartIndex);
      return true;
    }
    return false;
  }
}
