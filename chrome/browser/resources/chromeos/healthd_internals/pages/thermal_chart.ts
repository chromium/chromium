// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/iron-location/iron-location.js';
import '//resources/polymer/v3_0/iron-pages/iron-pages.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {HealthdApiThermalResult} from '../externs.js';

import {getTemplate} from './thermal_chart.html.js';

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

  updateThermalData(thermals: HealthdApiThermalResult[], _: number) {
    thermals.sort(sortThermals);
    // TODO(b/350423216): Handle the sorted `thermals` and timestamp.
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
