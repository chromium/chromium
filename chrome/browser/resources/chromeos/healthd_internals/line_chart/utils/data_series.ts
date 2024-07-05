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
    // TODO(b/350423216): To avoid hitting performance issue, we should keep a
    // limited number of data points and delete outdated data points.
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
}
