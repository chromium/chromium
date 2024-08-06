// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../line_chart/line_chart.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {HealthdInternalsLineChartElement} from '../line_chart/line_chart.js';
import type {DataSeries} from '../line_chart/utils/data_series.js';

import {getTemplate} from './generic_chart.html.js';
import {HealthdInternalsPage} from './utils/page_interface.js';
import {UiUpdateHelper} from './utils/ui_update_helper.js';

export interface HealthdInternalsGenericChartElement {
  $: {
    lineChart: HealthdInternalsLineChartElement,
  };
}

export class HealthdInternalsGenericChartElement extends PolymerElement
    implements HealthdInternalsPage {
  static get is() {
    return 'healthd-internals-generic-chart';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      chartHeader: {type: String},
    };
  }

  override connectedCallback() {
    super.connectedCallback();

    this.updateHelper = new UiUpdateHelper(() => {
      this.$.lineChart.updateEndTime(Date.now());
    });
  }

  // Header of the line chart.
  private chartHeader: string = '';

  // Helper for updating UI regularly. Init in `connectedCallback`.
  private updateHelper: UiUpdateHelper;

  setupChartHeader(header: string) {
    this.chartHeader = header;
  }

  initCanvasDrawer(units: string[], unitBase: number) {
    this.$.lineChart.initCanvasDrawer(units, unitBase);
  }

  setChartMaxValue(maxValue: number) {
    this.$.lineChart.setChartMaxValue(maxValue);
  }

  addDataSeries(dataSeriesList: DataSeries[]) {
    for (const dataSeries of dataSeriesList) {
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
    'healthd-internals-generic-chart': HealthdInternalsGenericChartElement;
  }
}

customElements.define(
    HealthdInternalsGenericChartElement.is,
    HealthdInternalsGenericChartElement);
