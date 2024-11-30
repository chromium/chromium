// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cr_button/cr_button.js';
import '//resources/ash/common/cr_elements/md_select.css.js';
import '../line_chart/line_chart.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CategoryTypeEnum, SystemTrendController} from '../../controller/system_trend_controller.js';
import {HealthdInternalsPage} from '../../utils/page_interface.js';
import {UiUpdateHelper} from '../../utils/ui_update_helper.js';
import type {HealthdInternalsLineChartElement} from '../line_chart/line_chart.js';

import {getTemplate} from './system_trend.html.js';

function toReadableDuration(timeMilliseconds: number): string {
  if (timeMilliseconds < 0) {
    console.warn('Failed to get positive duration.')
    return 'N/A';
  }

  const seconds = timeMilliseconds / 1000;
  const minutes = seconds / 60;
  const hours = minutes / 60;
  const formatTimeNumber = (input: number) => {
    return Math.round(input).toString().padStart(2, '0');
  };
  return `${formatTimeNumber(hours)}:${formatTimeNumber(minutes % 60)}:${
      formatTimeNumber(seconds % 60)}`;
}

export interface HealthdInternalsSystemTrendElement {
  $: {
    categorySelector: HTMLSelectElement,
    lineChart: HealthdInternalsLineChartElement,
  };
}

/**
 * The system trend page to provide historical data. Also handles user
 * interactions and configuration.
 */
export class HealthdInternalsSystemTrendElement extends PolymerElement
    implements HealthdInternalsPage {
  static get is() {
    return 'healthd-internals-system-trend';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      isSummaryTableDisplayed: {type: Boolean},
      displayedCategories: {type: Array},
      selectedCategory: {type: String},
      displayedStartTime: {type: String},
      displayedEndTime: {type: String},
      displayedDuration: {type: String},
    };
  }

  override connectedCallback() {
    super.connectedCallback();

    this.updateHelper = new UiUpdateHelper(() => {
      this.$.lineChart.update();
    });

    this.$.lineChart.addEventListener('time-range-changed', () => {
      this.updateDisplayedTimeInfo();
    });
  }

  // Controller for this UI element.
  private controller: SystemTrendController = new SystemTrendController(this);

  // Helper for updating UI regularly. Init in `connectedCallback`.
  private updateHelper: UiUpdateHelper;

  // Whether the chart summary table is displayed.
  private isSummaryTableDisplayed: boolean = true;

  // The available sources for system trend page.
  private readonly displayedCategories: CategoryTypeEnum[] = [
    CategoryTypeEnum.CPU_USAGE,
    CategoryTypeEnum.CPU_FREQUENCY,
    CategoryTypeEnum.MEMORY,
    CategoryTypeEnum.ZRAM,
    CategoryTypeEnum.BATTERY,
    CategoryTypeEnum.THERMAL,
    CategoryTypeEnum.CUSTOM,
  ];

  private selectedCategory: CategoryTypeEnum = this.displayedCategories[0];

  // The start and end time in the visible part of line chart.
  private displayedStartTime: string = '';
  private displayedEndTime: string = '';

  // The time duration for lines in the chart summary table.
  private displayedDuration: string = '';

  getController(): SystemTrendController {
    return this.controller;
  }

  updateVisibility(isVisible: boolean) {
    this.$.lineChart.updateVisibility(isVisible);
    this.updateHelper.updateVisibility(isVisible);
  }

  updateUiUpdateInterval(intervalSeconds: number) {
    this.updateHelper.updateUiUpdateInterval(intervalSeconds);
  }

  setupDataSeriesList() {
    this.$.lineChart.setupDataSeriesLists(
        this.selectedCategory, this.controller.getData(this.selectedCategory));
  }

  private updateDisplayedTimeInfo() {
    const [startTime, endTime] = this.$.lineChart.getVisibleTimeSpan();
    this.displayedStartTime = new Date(startTime).toLocaleTimeString();
    this.displayedEndTime = new Date(endTime).toLocaleTimeString();
    this.displayedDuration = toReadableDuration(endTime - startTime);
  }

  private toggleChartSummaryTable() {
    this.isSummaryTableDisplayed = !this.isSummaryTableDisplayed;
    this.$.lineChart.renderChartSummaryTable(this.isSummaryTableDisplayed);
  }

  private onCategoryChanged() {
    this.selectedCategory = this.$.categorySelector.value as CategoryTypeEnum;
    this.setupDataSeriesList();
    this.$.lineChart.update()
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-system-trend': HealthdInternalsSystemTrendElement;
  }
}

customElements.define(
    HealthdInternalsSystemTrendElement.is, HealthdInternalsSystemTrendElement);
