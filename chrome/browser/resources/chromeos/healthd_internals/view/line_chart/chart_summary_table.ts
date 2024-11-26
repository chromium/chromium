// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DataSeries} from '../../model/data_series.js';
import type {DataPointsStatistics} from '../../model/data_series.js';
import {toFixedFloat} from '../../utils/number_utils.js';

import {getTemplate} from './chart_summary_table.html.js';


/**
 * The data structure for each row in the chart summary table. Each row shows
 * details for each line in the visible part of the chart.
 */
interface DisplayedLineInfo {
  // Color of legend.
  legendColor: string;
  // Name of the line.
  name: string;
  // Whether the data is visible in the chart.
  isVisible: boolean;
  // The latest value of line.
  latestValue: number;
  // The minimum value of line.
  minValue: number;
  // The maximum value of line.
  maxValue: number;
  // The average value of line.
  averageValue: number;
}

function toReadableDuration(timeMilliseconds: number): string {
  if (timeMilliseconds < 0) {
    console.warn('Failed to get positive duration.')
    return 'N/A';
  }

  const seconds = timeMilliseconds / 1000;
  const minutes = seconds / 60;
  const hours = minutes / 60;
  const formatTimeNumber = (input: number) => {
    return Math.round(input).toString().padStart(2, '0');
  };
  return `${formatTimeNumber(hours)}:${formatTimeNumber(minutes % 60)}:${
      formatTimeNumber(seconds % 60)}`;
}

export class HealthdInternalsChartSummaryTableElement extends PolymerElement {
  static get is() {
    return 'healthd-internals-chart-summary-table';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      displayedData: {type: Array},
      displayedUnit: {type: String},
      displayedStartTime: {type: String},
      displayedEndTime: {type: String},
      displayedDuration: {type: String},
    };
  }

  // List of data series objects.
  private dataSeriesList: DataSeries[] = [];

  // Data displayed in the chart summary table.
  private displayedData: DisplayedLineInfo[] = [];

  // The displayed unit for lines in the chart summary table.
  private displayedUnit: string = '';

  // The start and end time in the visible part of line chart.
  private displayedStartTime: string = '';
  private displayedEndTime: string = '';

  // The time duration for lines in the chart summary table.
  private displayedDuration: string = '';

  // Set up the list of data series.
  setupDataSeriesList(dataSeriesList: DataSeries[]) {
    this.dataSeriesList = dataSeriesList;
    for (const dataSeries of dataSeriesList) {
      this.displayedData.push({
        legendColor: dataSeries.getColor(),
        name: dataSeries.getTitle(),
        isVisible: false,
        latestValue: 0,
        minValue: 0,
        maxValue: 0,
        averageValue: 0
      });
    }
    // Create a copy to trigger a change for the new row in table.
    this.set('displayedData', this.displayedData.slice());
  }

  /**
   * Update the displayed info in chart summary table.
   *
   * @param visibleStartTime - The timestamp of start time for visible chart.
   * @param visibleEndTime - The timestamp of end time for visible chart.
   * @param displayedUnit - The unit string to display.
   * @param unitScale - The multiplier for converting the displayed value to
   *                    real value.
   */
  updateDisplayedInfo(
      visibleStartTime: number, visibleEndTime: number, displayedUnit: string,
      unitScale: number) {
    this.displayedUnit = displayedUnit;
    this.displayedStartTime = new Date(visibleStartTime).toLocaleTimeString();
    this.displayedEndTime = new Date(visibleEndTime).toLocaleTimeString();
    this.displayedDuration =
        toReadableDuration(visibleEndTime - visibleStartTime);

    assert(this.dataSeriesList.length === this.displayedData.length);
    for (const [i, dataSeries] of this.dataSeriesList.entries()) {
      const statistics: DataPointsStatistics =
          dataSeries.getLatestStatistics(visibleStartTime, visibleEndTime);

      // Update the property by the `set` function to trigger a change.
      this.set(`displayedData.${i}.isVisible`, dataSeries.getVisible());
      this.set(
          `displayedData.${i}.latestValue`,
          toFixedFloat(statistics.latest / unitScale, 3));
      this.set(
          `displayedData.${i}.minValue`,
          toFixedFloat(statistics.min / unitScale, 3));
      this.set(
          `displayedData.${i}.maxValue`,
          toFixedFloat(statistics.max / unitScale, 3));
      this.set(
          `displayedData.${i}.averageValue`,
          toFixedFloat(statistics.average / unitScale, 3));
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-chart-summary-table':
        HealthdInternalsChartSummaryTableElement;
  }
}

customElements.define(
    HealthdInternalsChartSummaryTableElement.is,
    HealthdInternalsChartSummaryTableElement);
