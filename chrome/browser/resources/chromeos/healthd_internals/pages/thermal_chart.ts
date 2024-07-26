// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../line_chart/line_chart.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {HealthdInternalsLineChartElement} from '../line_chart/line_chart.js';
import type {DataSeries} from '../line_chart/utils/data_series.js';

import {getTemplate} from './thermal_chart.html.js';

export interface HealthdInternalsThermalChartElement {
  $: {
    lineChart: HealthdInternalsLineChartElement,
  };
}

export class HealthdInternalsThermalChartElement extends PolymerElement {
  static get is() {
    return 'healthd-internals-thermal-chart';
  }

  static get template() {
    return getTemplate();
  }

  override connectedCallback() {
    super.connectedCallback();

    const UNITBASE_NO_CARRY: number = 1;
    const UNIT_CELSIUS: string[] = ['C'];
    this.$.lineChart.initCanvasDrawer(UNIT_CELSIUS, UNITBASE_NO_CARRY);
  }


  // Whether the current page is visible.
  private isVisible: boolean = false;
  // The update internal for line chart in milliseconds.
  private updateInterval?: number = undefined;
  // The data fetching interval ID used for cancelling the running interval.
  private updateChartInternalId?: number = undefined;

  addDataSeries(thermalDataSeries: DataSeries[]) {
    for (const dataSeries of thermalDataSeries) {
      this.$.lineChart.addDataSeries(dataSeries);
    }
  }

  updateVisibility(isVisible: boolean) {
    this.isVisible = isVisible;
    this.$.lineChart.updateVisibility(isVisible);
    this.setupUpdateChartRequests();
  }

  updateUiUpdateInterval(intervalSeconds: number) {
    this.updateInterval = intervalSeconds * 1000;
    this.setupUpdateChartRequests();
  }

  private setupUpdateChartRequests() {
    this.cancelUpdateChartRequests();

    if (!this.isVisible || this.updateInterval === undefined ||
        this.updateInterval === 0) {
      return;
    }
    const updateChart = () => this.$.lineChart.updateEndTime(Date.now());
    this.updateChartInternalId = setInterval(updateChart, this.updateInterval);
    updateChart();
  }

  private cancelUpdateChartRequests() {
    if (this.updateChartInternalId === undefined) {
      return;
    }
    clearInterval(this.updateChartInternalId);
    this.updateChartInternalId = undefined;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-thermal-chart': HealthdInternalsThermalChartElement;
  }
}

customElements.define(
    HealthdInternalsThermalChartElement.is,
    HealthdInternalsThermalChartElement);
