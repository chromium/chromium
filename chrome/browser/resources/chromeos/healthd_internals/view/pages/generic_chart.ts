// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cr_button/cr_button.js';
import '../line_chart/line_chart.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {DataSeries} from '../../model/data_series.js';
import {HealthdInternalsPage} from '../../utils/page_interface.js';
import {UiUpdateHelper} from '../../utils/ui_update_helper.js';
import type {HealthdInternalsLineChartElement} from '../line_chart/line_chart.js';

import {getTemplate} from './generic_chart.html.js';

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
      isSummaryTableDisplayed: {type: Boolean},
    };
  }

  override connectedCallback() {
    super.connectedCallback();

    this.updateHelper = new UiUpdateHelper(() => {
      this.$.lineChart.update();
    });
  }

  // Header of the line chart.
  private chartHeader: string = '';

  // Helper for updating UI regularly. Init in `connectedCallback`.
  private updateHelper: UiUpdateHelper;

  // Whether the chart summary table is displayed.
  private isSummaryTableDisplayed: boolean = true;

  setupChartHeader(header: string) {
    this.chartHeader = header;
  }

  initCanvasDrawer(units: string[], unitBase: number) {
    this.$.lineChart.getController().initUnitLabel(units, unitBase);
  }

  setChartMaxValue(maxValue: number) {
    this.$.lineChart.getController().setChartMaxValue(maxValue);
  }

  setupDataSeriesList(dataSeriesList: DataSeries[]) {
    this.$.lineChart.setupDataSeriesList(dataSeriesList);
  }

  updateVisibility(isVisible: boolean) {
    this.$.lineChart.updateVisibility(isVisible);
    this.updateHelper.updateVisibility(isVisible);
  }

  updateUiUpdateInterval(intervalSeconds: number) {
    this.updateHelper.updateUiUpdateInterval(intervalSeconds);
  }

  private toggleChartSummaryTable() {
    this.isSummaryTableDisplayed = !this.isSummaryTableDisplayed;
    this.$.lineChart.renderChartSummaryTable(this.isSummaryTableDisplayed);
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
