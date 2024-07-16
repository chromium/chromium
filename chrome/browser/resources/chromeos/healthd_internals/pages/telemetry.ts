// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../info_card/power_thermal_card.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {HealthdApiTelemetryResult} from '../externs.js';
import type {HealthdInternalsPowerThermalCardElement} from '../info_card/power_thermal_card.js';

import {getTemplate} from './telemetry.html.js';

export interface HealthdInternalsTelemetryElement {
  $: {
    powerThermalCard: HealthdInternalsPowerThermalCardElement,
  };
}

export class HealthdInternalsTelemetryElement extends PolymerElement {
  static get is() {
    return 'healthd-internals-telemetry';
  }

  static get template() {
    return getTemplate();
  }

  updateTelemetryData(data: HealthdApiTelemetryResult) {
    this.$.powerThermalCard.updateTelemetryData(data);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-telemetry': HealthdInternalsTelemetryElement;
  }
}

customElements.define(
    HealthdInternalsTelemetryElement.is, HealthdInternalsTelemetryElement);
