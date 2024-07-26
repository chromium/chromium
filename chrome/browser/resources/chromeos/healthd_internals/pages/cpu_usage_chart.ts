// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../line_chart/line_chart.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {HealthdInternalsLineChartElement} from '../line_chart/line_chart.js';
import type {DataSeries} from '../line_chart/utils/data_series.js';

import {getTemplate} from './cpu_usage_chart.html.js';

export interface HealthdInternalsCpuUsageChartElement {
  $: {
    lineChart: HealthdInternalsLineChartElement,
  };
}

export class HealthdInternalsCpuUsageChartElement extends PolymerElement {
  static get is() {
    return 'healthd-internals-cpu-usage-chart';
  }

  static get template() {
    return getTemplate();
  }

  override connectedCallback() {
    super.connectedCallback();

    const UNITBASE_NO_CARRY: number = 1;
    const UNIT_PERCENTAGE: string[] = ['%'];
    this.$.lineChart.initCanvasDrawer(UNIT_PERCENTAGE, UNITBASE_NO_CARRY);
    this.$.lineChart.setChartMaxValue(100);
  }

  // Whether the current page is visible.
  private isVisible: boolean = false;
  // The update internal for line chart in milliseconds.
  private updateInterval?: number = undefined;
  // The data fetching interval ID used for cancelling the running interval.
  private updateChartInternalId?: number = undefined;

  addDataSeries(cpuUsageDataSeries: DataSeries[]) {
    for (const dataSeries of cpuUsageDataSeries) {
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
    'healthd-internals-cpu-usage-chart': HealthdInternalsCpuUsageChartElement;
  }
}

customElements.define(
    HealthdInternalsCpuUsageChartElement.is,
    HealthdInternalsCpuUsageChartElement);
