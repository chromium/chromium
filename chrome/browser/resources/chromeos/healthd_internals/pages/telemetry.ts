// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../info_card/cpu_card.js';
import '../info_card/fan_card.js';
import '../info_card/memory_card.js';
import '../info_card/power_card.js';
import '../info_card/thermal_card.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {HealthdApiTelemetryResult} from '../externs.js';
import type {HealthdInternalsCpuCardElement} from '../info_card/cpu_card.js';
import type {HealthdInternalsFanCardElement} from '../info_card/fan_card.js';
import type {HealthdInternalsMemoryCardElement} from '../info_card/memory_card.js';
import type {HealthdInternalsPowerCardElement} from '../info_card/power_card.js';
import type {HealthdInternalsThermalCardElement} from '../info_card/thermal_card.js';

import {getTemplate} from './telemetry.html.js';

export interface HealthdInternalsTelemetryElement {
  $: {
    cpuCard: HealthdInternalsCpuCardElement,
    fanCard: HealthdInternalsFanCardElement,
    memoryCard: HealthdInternalsMemoryCardElement,
    powerCard: HealthdInternalsPowerCardElement,
    thermalCard: HealthdInternalsThermalCardElement,
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
    this.$.cpuCard.updateTelemetryData(data);
    this.$.fanCard.updateTelemetryData(data);
    this.$.memoryCard.updateTelemetryData(data);
    this.$.powerCard.updateTelemetryData(data);
    this.$.thermalCard.updateTelemetryData(data);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-telemetry': HealthdInternalsTelemetryElement;
  }
}

customElements.define(
    HealthdInternalsTelemetryElement.is, HealthdInternalsTelemetryElement);
