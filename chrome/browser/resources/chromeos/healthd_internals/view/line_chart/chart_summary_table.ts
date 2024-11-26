// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getLineChartColor} from '../../controller/line_chart_controller.js';
import {DataSeries} from '../../model/data_series.js';
import type {DataPointsStatistics} from '../../model/data_series.js';
import {toFixedFloat} from '../../utils/number_utils.js';

import {getTemplate} from './chart_summary_table.html.js';


/**
 * The data structure for each row in the chart summary table. Each row shows
 * details for each line in the visible part of the chart.
 */
export interface DisplayedLineInfo {
  // Color of legend.
  legendColor: string;
  // Name of the line.
  name: string;
  // Whether the data is visible in the chart.
  isVisible: boolean;
  // The unit string to display.
  displayedUnit: string;
  // The latest value of line.
  latestValue: number;
  // The minimum value of line.
  minValue: number;
  // The maximum value of line.
  maxValue: number;
  // The average value of line.
  averageValue: number;
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
    };
  }

  // Data displayed in the chart summary table.
  private displayedData: DisplayedLineInfo[] = [];

  /**
   * Update displayed data.
   */
  updateSummaryInfo(data: DisplayedLineInfo[]) {
    this.displayedData = [];
    for (let info of data) {
      info.latestValue = parseFloat(toFixedFloat(info.latestValue, 3));
      info.minValue = parseFloat(toFixedFloat(info.minValue, 3));
      info.maxValue = parseFloat(toFixedFloat(info.maxValue, 3));
      info.averageValue = parseFloat(toFixedFloat(info.averageValue, 3));
      this.displayedData.push(info);
    }

    // Create a copy to trigger a change for the new row in table.
    this.set('displayedData', this.displayedData.slice());
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
