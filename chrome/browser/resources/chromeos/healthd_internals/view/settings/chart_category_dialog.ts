// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cr_button/cr_button.js';
import '//resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import './data_series_checkbox.js';

import type {CrDialogElement} from '//resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SystemTrendController} from '../../controller/system_trend_controller.js';
import {CategoryTypeEnum} from '../../controller/system_trend_controller.js';

import {getTemplate} from './chart_category_dialog.html.js';
import type {HealthdInternalsDataSeriesCheckboxElement} from './data_series_checkbox.js';

export interface HealthdInternalsChartCategoryDialogElement {
  $: {
    dialog: CrDialogElement,
    batteryCheckbox: HealthdInternalsDataSeriesCheckboxElement,
    cpuFrequencyCheckbox: HealthdInternalsDataSeriesCheckboxElement,
    cpuUsageCheckbox: HealthdInternalsDataSeriesCheckboxElement,
    memoryCheckbox: HealthdInternalsDataSeriesCheckboxElement,
    thermalCheckbox: HealthdInternalsDataSeriesCheckboxElement,
    zramCheckbox: HealthdInternalsDataSeriesCheckboxElement,
  };
}

export class HealthdInternalsChartCategoryDialogElement extends PolymerElement {
  static get is() {
    return 'healthd-internals-chart-category-dialog';
  }

  static get template() {
    return getTemplate();
  }

  private controller: SystemTrendController|null = null;

  openDialog(controller: SystemTrendController) {
    this.controller = controller;
    this.$.batteryCheckbox.init(
        'Battery', this.controller.getData(CategoryTypeEnum.BATTERY));
    this.$.cpuFrequencyCheckbox.init(
        'CPU Frequency',
        this.controller.getData(CategoryTypeEnum.CPU_FREQUENCY));
    this.$.cpuUsageCheckbox.init(
        'CPU Usage', this.controller.getData(CategoryTypeEnum.CPU_USAGE));
    this.$.memoryCheckbox.init(
        'Memory', this.controller.getData(CategoryTypeEnum.MEMORY));
    this.$.thermalCheckbox.init(
        'Thermal', this.controller.getData(CategoryTypeEnum.THERMAL));
    this.$.zramCheckbox.init(
        'Zram', this.controller.getData(CategoryTypeEnum.ZRAM));
    this.$.dialog.showModal();
  }


  private onConfirmButtonClicked() {
    this.controller?.setSelectedIndices(
        CategoryTypeEnum.BATTERY, this.$.batteryCheckbox.getSelectedIndices());
    this.controller?.setSelectedIndices(
        CategoryTypeEnum.CPU_FREQUENCY,
        this.$.cpuFrequencyCheckbox.getSelectedIndices());
    this.controller?.setSelectedIndices(
        CategoryTypeEnum.CPU_USAGE,
        this.$.cpuUsageCheckbox.getSelectedIndices());
    this.controller?.setSelectedIndices(
        CategoryTypeEnum.MEMORY, this.$.memoryCheckbox.getSelectedIndices());
    this.controller?.setSelectedIndices(
        CategoryTypeEnum.THERMAL, this.$.thermalCheckbox.getSelectedIndices());
    this.controller?.setSelectedIndices(
        CategoryTypeEnum.ZRAM, this.$.zramCheckbox.getSelectedIndices());

    this.dispatchEvent(new CustomEvent(
        'custom-data-updated', {bubbles: true, composed: true}));
    this.controller = null;
    this.$.dialog.close();
  }

  private onCancelButtonClicked() {
    this.controller = null;
    this.$.dialog.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-chart-category-dialog':
        HealthdInternalsChartCategoryDialogElement;
  }
}

customElements.define(
    HealthdInternalsChartCategoryDialogElement.is,
    HealthdInternalsChartCategoryDialogElement);
