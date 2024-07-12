// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../line_chart/line_chart.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LINE_CHART_COLOR_SET} from '../constants.js';
import {HealthdApiBatteryResult} from '../externs.js';
import type {HealthdInternalsLineChartElement} from '../line_chart/line_chart.js';
import {DataSeries} from '../line_chart/utils/data_series.js';

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

  // The historical battery data for displaying line chart.
  private batteryDataSeries: DataSeries[] = [];

  updateBatteryData(battery: HealthdApiBatteryResult, timestamp: number) {
    if (this.batteryDataSeries.length === 0) {
      this.initBatteryDataSeries();
    }

    this.batteryDataSeries[0].addDataPoint(battery.voltageNow, timestamp);
    this.batteryDataSeries[1].addDataPoint(battery.chargeNow, timestamp);
    this.batteryDataSeries[2].addDataPoint(battery.currentNow, timestamp);

    this.$.lineChart.updateEndTime(timestamp);
  }

  // Init the `batteryDataSeries`.
  private initBatteryDataSeries() {
    this.batteryDataSeries[0] =
        new DataSeries('Voltage (V)', LINE_CHART_COLOR_SET[0]);
    this.batteryDataSeries[1] =
        new DataSeries('Charge (Ah)', LINE_CHART_COLOR_SET[1]);
    this.batteryDataSeries[2] =
        new DataSeries('Current (A)', LINE_CHART_COLOR_SET[2]);
    this.initBatteryPage();
  }

  // Init the battery page.
  private initBatteryPage() {
    const UNITBASE_NO_CARRY: number = 1;
    const UNIT_PURE_NUMBER: string[] = [''];
    this.$.lineChart.initCanvasDrawer(UNIT_PURE_NUMBER, UNITBASE_NO_CARRY);

    const batteryDataSeries: DataSeries[] = this.batteryDataSeries;
    for (let i = 0; i < batteryDataSeries.length; ++i) {
      this.$.lineChart.addDataSeries(batteryDataSeries[i]);
    }
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
