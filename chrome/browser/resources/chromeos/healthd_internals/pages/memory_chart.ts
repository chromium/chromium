// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../line_chart/line_chart.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {HealthdInternalsLineChartElement} from '../line_chart/line_chart.js';
import type {DataSeries} from '../line_chart/utils/data_series.js';

import {getTemplate} from './memory_chart.html.js';
import {HealthdInternalsPage} from './utils/page_interface.js';
import {UiUpdateHelper} from './utils/ui_update_helper.js';

export interface HealthdInternalsMemoryChartElement {
  $: {
    lineChart: HealthdInternalsLineChartElement,
  };
}

export class HealthdInternalsMemoryChartElement extends PolymerElement
    implements HealthdInternalsPage {
  static get is() {
    return 'healthd-internals-memory-chart';
  }

  static get template() {
    return getTemplate();
  }

  override connectedCallback() {
    super.connectedCallback();

    const UNITBASE_BYTE: number = 1024;
    const UNIT_MEMORY_BYTES: string[] = ['KiB', 'MiB', 'GiB'];
    this.$.lineChart.initCanvasDrawer(UNIT_MEMORY_BYTES, UNITBASE_BYTE);

    this.updateHelper = new UiUpdateHelper(() => {
      this.$.lineChart.updateEndTime(Date.now());
    });
  }

  // Helper for updating UI regularly. Init in `connectedCallback`.
  private updateHelper: UiUpdateHelper;

  addDataSeries(memoryDataSeries: DataSeries[]) {
    for (const dataSeries of memoryDataSeries) {
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
    'healthd-internals-memory-chart': HealthdInternalsMemoryChartElement;
  }
}

customElements.define(
    HealthdInternalsMemoryChartElement.is, HealthdInternalsMemoryChartElement);
