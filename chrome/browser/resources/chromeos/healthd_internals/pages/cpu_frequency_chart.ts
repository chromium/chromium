// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../line_chart/line_chart.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {HealthdInternalsLineChartElement} from '../line_chart/line_chart.js';
import type {DataSeries} from '../line_chart/utils/data_series.js';

import {getTemplate} from './cpu_frequency_chart.html.js';

export interface HealthdInternalsCpuFrequencyChartElement {
  $: {
    lineChart: HealthdInternalsLineChartElement,
  };
}

export class HealthdInternalsCpuFrequencyChartElement extends PolymerElement {
  static get is() {
    return 'healthd-internals-cpu-frequency-chart';
  }

  static get template() {
    return getTemplate();
  }

  override connectedCallback() {
    super.connectedCallback();

    const UNITBASE_FREQUENCY: number = 1000;
    const UNIT_FREQUENCY: string[] = ['kHz', 'mHz', 'GHz'];
    this.$.lineChart.initCanvasDrawer(UNIT_FREQUENCY, UNITBASE_FREQUENCY);
  }

  addDataSeries(cpuFrequencyDataSeries: DataSeries[]) {
    for (const dataSeries of cpuFrequencyDataSeries) {
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
    'healthd-internals-cpu-frequency-chart':
        HealthdInternalsCpuFrequencyChartElement;
  }
}

customElements.define(
    HealthdInternalsCpuFrequencyChartElement.is,
    HealthdInternalsCpuFrequencyChartElement);
