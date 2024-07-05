// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/iron-location/iron-location.js';
import '//resources/polymer/v3_0/iron-pages/iron-pages.js';
import '../line_chart/line_chart.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LINE_CHART_COLOR_SET} from '../constants.js';
import {HealthdApiThermalResult} from '../externs.js';
import type {HealthdInternalsLineChartElement} from '../line_chart/line_chart.js';
import {DataSeries} from '../line_chart/utils/data_series.js';

import {getTemplate} from './thermal_chart.html.js';

export interface HealthdInternalsThermalChartElement {
  $: {
    lineChart: HealthdInternalsLineChartElement,
  };
}

function sortThermals(
    first: HealthdApiThermalResult, second: HealthdApiThermalResult): number {
  if (first.source === second.source) {
    return first.name.localeCompare(second.name);
  }
  return first.source.localeCompare(second.source);
}

export class HealthdInternalsThermalChartElement extends PolymerElement {
  static get is() {
    return 'healthd-internals-thermal-chart';
  }

  static get template() {
    return getTemplate();
  }

  // The historical thermal data for displaying line chart.
  private thermalDataSeries: DataSeries[] = [];

  updateThermalData(thermals: HealthdApiThermalResult[], timestamp: number) {
    thermals.sort(sortThermals);
    if (this.thermalDataSeries.length === 0) {
      this.initThermalDataSeries(thermals);
    }

    if (thermals.length !== this.thermalDataSeries.length) {
      console.warn('Thermal data: Number of thermal sensors changed.');
      return;
    }

    for (let i = 0; i < thermals.length; ++i) {
      this.thermalDataSeries[i].addDataPoint(
          thermals[i].temperatureCelsius, timestamp);
    }

    this.$.lineChart.updateEndTime(timestamp);
  }

  // Init the `thermalDataSeries`.
  private initThermalDataSeries(thermals: HealthdApiThermalResult[]) {
    for (let i = 0; i < thermals.length; ++i) {
      const colorIdx: number = i % LINE_CHART_COLOR_SET.length;
      this.thermalDataSeries[i] = new DataSeries(
          thermals[i].name + ' (' + thermals[i].source + ')',
          LINE_CHART_COLOR_SET[colorIdx]);
    }
    this.initThermalPage();
  }

  // Init the thermal page.
  private initThermalPage() {
    const UNITBASE_NO_CARRY: number = 1;
    const UNIT_CELSIUS: string[] = ['C'];
    this.$.lineChart.initCanvasDrawer(UNIT_CELSIUS, UNITBASE_NO_CARRY);

    const thermalDataSeries: DataSeries[] = this.thermalDataSeries;
    for (let i = 0; i < thermalDataSeries.length; ++i) {
      this.$.lineChart.addDataSeries(thermalDataSeries[i]);
    }
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
