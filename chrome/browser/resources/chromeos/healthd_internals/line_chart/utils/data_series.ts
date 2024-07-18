// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The interface of data points displayed in the line chart.
 */
interface DataPoint {
  value: number;
  time: number;
}

/**
 * The implementation of linear interpolation.
 */
function linearInterpolation(
    x1: number, y1: number, x2: number, y2: number, x: number): number {
  if (x1 === x2) {
    return (y1 + y2) / 2;
  }
  const ratio: number = (x - x1) / (x2 - x1);
  return (y2 - y1) * ratio + y1;
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

  // Cache data.
  private cacheStartTime: number = 0;
  private cacheStepSize: number = 0;
  private cacheValues: Array<number|null> = [];
  private cacheMaxValue: number = 0;

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
    this.clearCacheValue();
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
   * Get the displayed values to draw on line chart.
   * @param startTime - The time of first point.
   * @param stepSize - The step size between two value points.
   * @param count - The number of values.
   * @return - If a cell of the array is null, it means that there is no any
   *           data point in this interval.
   */
  getDisplayedValues(startTime: number, stepSize: number, count: number):
      Array<number|null> {
    if (!this.isVisible) {
      return [];
    }
    this.updateCacheValues(startTime, stepSize, count);
    return this.cacheValues;
  }

  // Get the maximum value in the query interval.
  getMaxValue(startTime: number, stepSize: number, count: number): number {
    if (!this.isVisible) {
      return 0;
    }
    this.updateCacheValues(startTime, stepSize, count);
    return this.cacheMaxValue;
  }

  // Implementation of querying the values and the max value.
  private updateCacheValues(
      startTime: number, stepSize: number, count: number) {
    if (this.cacheStartTime === startTime && this.cacheStepSize === stepSize &&
        this.cacheValues.length === count) {
      return;
    }

    const values: Array<number|null> = [];
    values.length = count;
    let endTime: number = startTime;
    const firstIndex: number = this.findLowerBoundPointIndex(startTime);
    let nextIndex: number = firstIndex;
    let maxValue: number = 0;

    for (let i = 0; i < count; ++i) {
      endTime += stepSize;
      const result: {value: number|null, nextIndex: number} =
          this.getSampleValue(nextIndex, endTime);
      values[i] = result.value;
      nextIndex = result.nextIndex;
      maxValue = Math.max(maxValue, values[i] ?? Number.MIN_VALUE);
    }

    this.cacheValues = values;
    this.cacheStartTime = startTime;
    this.cacheStepSize = stepSize;
    this.cacheMaxValue = maxValue;

    this.backfillValuePoint(0, firstIndex, startTime);
    const lastIndex: number = nextIndex;
    const lastPointStartTime: number = endTime - stepSize;
    this.backfillValuePoint(count - 1, lastIndex, lastPointStartTime);
  }

  private clearCacheValue() {
    this.cacheValues = [];
  }

  /**
   * Find the index of lower bound point by simple binary search.
   */
  findLowerBoundPointIndex(time: number): number {
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
   * Get a single sample value from `firstIndex` to `endTime`. Return the value
   * and the next index of the points.
   * If there are many data points in the query interval, return their average.
   * If there is no data point in the query interval, return null.
   */
  getSampleValue(firstIndex: number, endTime: number):
      {value: number|null, nextIndex: number} {
    let nextIndex: number = firstIndex;
    let currentValueSum: number = 0;
    let currentValueNum: number = 0;
    while (nextIndex < this.dataPoints.length) {
      const point: DataPoint = this.dataPoints[nextIndex];
      if (point.time >= endTime) {
        break;
      }
      currentValueSum += point.value;
      ++currentValueNum;
      ++nextIndex;
    }

    let value: number|null = null;
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
   *
   * @param valueIndex - The index of the value we want to fillback.
   * @param dataIndex - The index of the data point we need.
   * @param boundaryTime - Time we want to fillback the value.
   */
  private backfillValuePoint(
      valueIndex: number, dataIndex: number, boundaryTime: number) {
    const values: Array<number|null> = this.cacheValues;
    if (values[valueIndex] == null && dataIndex > 0 &&
        dataIndex < this.dataPoints.length) {
      values[valueIndex] = this.dataPointLinearInterpolation(
          this.dataPoints[dataIndex - 1], this.dataPoints[dataIndex],
          boundaryTime);
      this.cacheMaxValue =
          Math.max(this.cacheMaxValue, values[valueIndex] ?? Number.MIN_VALUE);
    }
  }

  /**
   * Do the linear interpolation for the data points.
   *
   * @param position - The position we want to insert the new value.
   */
  private dataPointLinearInterpolation(
      pointA: DataPoint, pointB: DataPoint, position: number): number {
    return linearInterpolation(
        this.getTimeOnLattice(pointA.time), pointA.value,
        this.getTimeOnLattice(pointB.time), pointB.value,
        this.getTimeOnLattice(position));
  }

  /**
   * When drawing the points of the line chart, we don't really draw the point
   * at the real position. Instead, we split the time axis into multiple
   * interval, and draw them on the edge of the interval. This function can find
   * the edge time of a interval the original time fall in.
   */
  private getTimeOnLattice(time: number): number {
    const startTime: number = this.cacheStartTime;
    const stepSize: number = this.cacheStepSize;
    return Math.floor((time - startTime) / stepSize) * stepSize;
  }

  /**
   * Filter out data points which the `time` field is earlier than startTime.
   */
  removeOutdatedData(startTime: number) {
    // TODO(b/353871773): Improve the performance of data points removing.
    let numRemovedPoints: number = 0;
    for (let i = 0; i < this.dataPoints.length; ++i) {
      if (this.dataPoints[i].time <= startTime) {
        numRemovedPoints += 1;
      } else {
        break;
      }
    }
    if (numRemovedPoints !== 0) {
      this.dataPoints.splice(0, numRemovedPoints);
    }
  }
}
