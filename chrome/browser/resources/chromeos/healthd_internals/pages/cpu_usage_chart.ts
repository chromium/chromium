// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../line_chart/line_chart.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {HealthdInternalsLineChartElement} from '../line_chart/line_chart.js';
import type {DataSeries} from '../line_chart/utils/data_series.js';

import {getTemplate} from './cpu_usage_chart.html.js';
import {HealthdInternalsPage} from './utils/page_interface.js';
import {UiUpdateHelper} from './utils/ui_update_helper.js';

export interface HealthdInternalsCpuUsageChartElement {
  $: {
    lineChart: HealthdInternalsLineChartElement,
  };
}

export class HealthdInternalsCpuUsageChartElement extends PolymerElement
    implements HealthdInternalsPage {
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

    this.updateHelper = new UiUpdateHelper(() => {
      this.$.lineChart.updateEndTime(Date.now());
    });
  }

  // Helper for updating UI regularly. Init in `connectedCallback`.
  private updateHelper: UiUpdateHelper;

  addDataSeries(cpuUsageDataSeries: DataSeries[]) {
    for (const dataSeries of cpuUsageDataSeries) {
      this.$.lineChart.addDataSeries(dataSeries);
    }
  }

  updateStartTime(startTime: number) {
    this.$.lineChart.updateStartTime(startTime);
  }

  updateVisibility(isVisible: boolean) {
    this.$.lineChart.updateVisibility(isVisible);
    this.updateHelper.updateVisibility(isVisible);
  }

  updateUiUpdateInterval(intervalSeconds: number) {
    this.updateHelper.updateUiUpdateInterval(intervalSeconds);
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
