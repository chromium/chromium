// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';

import {assert} from '//resources/js/assert.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {DataSeriesList} from '../../controller/system_trend_controller.js';

import {getTemplate} from './data_series_checkbox.html.js';

interface DataSeriesCheckboxData {
  name: string;
  isChecked: boolean;
}

export class HealthdInternalsDataSeriesCheckboxElement extends PolymerElement {
  static get is() {
    return 'healthd-internals-data-series-checkbox';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      checkboxTitle: {type: String},
      checkboxData: {type: Array},
    };
  }

  // Set in `init`.
  private checkboxTitle: string = '';

  // Stored the data from checkboxes.
  private checkboxData: DataSeriesCheckboxData[] = [];

  /**
   * Init the checkboxes.
   *
   * @param title - Displayed title for checkboxes.
   * @param data - Used to collect data name and selected indices.
   */
  init(title: string, data: DataSeriesList[]) {
    assert(data.length === 1);
    this.checkboxTitle = title;
    this.checkboxData = [];
    for (const dataSeries of data[0].dataList) {
      this.checkboxData.push({name: dataSeries.getTitle(), isChecked: false});
    }
    for (const index of data[0].selectedIndices) {
      this.checkboxData[index].isChecked = true;
    }

    // Create a copy to trigger a change for the new row in table.
    this.set('checkboxData', this.checkboxData.slice());
  }

  /**
   * Gets the selected indices from the `checkboxData`.
   *
   * @returns - List of selected indices.
   */
  getSelectedIndices(): number[] {
    const output: number[] = [];
    for (const [index, data] of this.checkboxData.entries()) {
      if (data.isChecked) {
        output.push(index);
      }
    }
    return output;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-data-series-checkbox':
        HealthdInternalsDataSeriesCheckboxElement;
  }
}

customElements.define(
    HealthdInternalsDataSeriesCheckboxElement.is,
    HealthdInternalsDataSeriesCheckboxElement);
