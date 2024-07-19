// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../line_chart/line_chart.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {HealthdInternalsLineChartElement} from '../line_chart/line_chart.js';
import type {DataSeries} from '../line_chart/utils/data_series.js';

import {getTemplate} from './battery_chart.html.js';

export interface HealthdInternalsBatteryChartElement {
  $: {
    lineChart: HealthdInternalsLineChartElement,
  };
}

export class HealthdInternalsBatteryChartElement extends PolymerElement {
  static get is() {
    return 'healthd-internals-battery-chart';
  }

  static get template() {
    return getTemplate();
  }

  override connectedCallback() {
    super.connectedCallback();

    const UNITBASE_NO_CARRY: number = 1;
    const UNIT_PURE_NUMBER: string[] = [''];
    this.$.lineChart.initCanvasDrawer(UNIT_PURE_NUMBER, UNITBASE_NO_CARRY);
  }

  addDataSeries(batteryDataSeries: DataSeries[]) {
    for (const dataSeries of batteryDataSeries) {
      this.$.lineChart.addDataSeries(dataSeries);
    }
  }

  updateEndTime(timestamp: number) {
    this.$.lineChart.updateEndTime(timestamp);
  }

  updateVisibility(isVisble: boolean) {
    this.$.lineChart.updateVisibility(isVisble);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-battery-chart': HealthdInternalsBatteryChartElement;
  }
}

customElements.define(
    HealthdInternalsBatteryChartElement.is,
    HealthdInternalsBatteryChartElement);
