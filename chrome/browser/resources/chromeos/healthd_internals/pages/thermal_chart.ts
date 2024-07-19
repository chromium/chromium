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

  addDataSeries(thermalDataSeries: DataSeries[]) {
    for (const dataSeries of thermalDataSeries) {
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
    'healthd-internals-thermal-chart': HealthdInternalsThermalChartElement;
  }
}

customElements.define(
    HealthdInternalsThermalChartElement.is,
    HealthdInternalsThermalChartElement);
