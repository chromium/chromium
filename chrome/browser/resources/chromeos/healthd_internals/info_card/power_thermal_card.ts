// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './info_card.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {HealthdApiTelemetryResult} from '../externs.js';

import type {HealthdInternalsInfoCardElement} from './info_card.js';
import {getTemplate} from './power_thermal_card.html.js';

export interface HealthdInternalsPowerThermalCardElement {
  $: {
    infoCard: HealthdInternalsInfoCardElement,
  };
}

export class HealthdInternalsPowerThermalCardElement extends PolymerElement {
  static get is() {
    return 'healthd-internals-power-thermal-card';
  }

  static get template() {
    return getTemplate();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.$.infoCard.appendCardRow('BATTERY');
    this.$.infoCard.appendCardRow('THERMALS');
  }

  updateTelemetryData(data: HealthdApiTelemetryResult) {
    this.$.infoCard.updateDisplayedInfo(0, data.battery);
    this.$.infoCard.updateDisplayedInfo(1, data.thermals);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-power-thermal-card':
        HealthdInternalsPowerThermalCardElement;
  }
}

customElements.define(
    HealthdInternalsPowerThermalCardElement.is,
    HealthdInternalsPowerThermalCardElement);
