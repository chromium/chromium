// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../line_chart/line_chart.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LINE_CHART_COLOR_SET} from '../constants.js';
import {HealthdApiCpuResult, HealthdApiLogicalCpuResult} from '../externs.js';
import type {HealthdInternalsLineChartElement} from '../line_chart/line_chart.js';
import {DataSeries} from '../line_chart/utils/data_series.js';

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

  // The historical frequency data of logical CPUs for displaying line chart.
  private cpuDataSeries: DataSeries[] = [];

  updateCpuData(cpu: HealthdApiCpuResult, timestamp: number) {
    if (this.cpuDataSeries.length === 0) {
      this.initCpuDataSeries(cpu);
    }

    let count: number = 0;
    for (let physicalCpuId: number = 0; physicalCpuId < cpu.physicalCpus.length;
         ++physicalCpuId) {
      const logicalCpus: HealthdApiLogicalCpuResult[] =
          cpu.physicalCpus[physicalCpuId].logicalCpus;
      for (let logicalCpuId: number = 0; logicalCpuId < logicalCpus.length;
           ++logicalCpuId) {
        this.cpuDataSeries[count].addDataPoint(
            parseInt(logicalCpus[logicalCpuId].frequency.current), timestamp);
        count += 1;
      }
    }

    this.$.lineChart.updateEndTime(timestamp);
  }

  // Init the `cpuDataSeries`.
  private initCpuDataSeries(cpu: HealthdApiCpuResult) {
    let count: number = 0;
    for (let physicalCpuId: number = 0; physicalCpuId < cpu.physicalCpus.length;
         ++physicalCpuId) {
      const logicalCpus: HealthdApiLogicalCpuResult[] =
          cpu.physicalCpus[physicalCpuId].logicalCpus;
      for (let logicalCpuId: number = 0; logicalCpuId < logicalCpus.length;
           ++logicalCpuId) {
        this.cpuDataSeries[count] = new DataSeries(
            `CPU #${physicalCpuId}-${logicalCpuId}`,
            LINE_CHART_COLOR_SET[count]);
        count += 1;
      }
    }

    this.initCpuPage();
  }

  // Init the CPU page.
  private initCpuPage() {
    const UNITBASE_FREQUENCY: number = 1000;
    const UNIT_FREQUENCY: string[] = ['kHz', 'mHz', 'GHz'];
    this.$.lineChart.initCanvasDrawer(UNIT_FREQUENCY, UNITBASE_FREQUENCY);

    const cpuDataSeries: DataSeries[] = this.cpuDataSeries;
    for (let i = 0; i < cpuDataSeries.length; ++i) {
      this.$.lineChart.addDataSeries(cpuDataSeries[i]);
    }
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
