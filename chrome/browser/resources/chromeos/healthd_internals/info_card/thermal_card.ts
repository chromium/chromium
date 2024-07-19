// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './info_card.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {HealthdApiTelemetryResult, HealthdApiThermalResult} from '../externs.js';

import type {HealthdInternalsInfoCardElement} from './info_card.js';
import {getTemplate} from './thermal_card.html.js';

export interface HealthdInternalsThermalCardElement {
  $: {
    infoCard: HealthdInternalsInfoCardElement,
  };
}

function filterThermalsBySource(
    thermals: HealthdApiThermalResult[], targetSource: string) {
  return thermals.filter(item => item.source === targetSource)
      .map(({source: _, ...rest}) => rest);
}

export class HealthdInternalsThermalCardElement extends PolymerElement {
  static get is() {
    return 'healthd-internals-thermal-card';
  }

  static get template() {
    return getTemplate();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.$.infoCard.appendCardRow('EC');
    this.$.infoCard.appendCardRow('SYSFS (thermal_zone)');
    this.$.infoCard.appendCardRow('CPU / SYSFS (hwmon)');
  }

  updateTelemetryData(data: HealthdApiTelemetryResult) {
    this.$.infoCard.updateDisplayedInfo(
        0, filterThermalsBySource(data.thermals, 'EC'));
    this.$.infoCard.updateDisplayedInfo(
        1, filterThermalsBySource(data.thermals, 'SysFs'));
    this.$.infoCard.updateDisplayedInfo(2, data.cpu.temperatureChannels);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-thermal-card': HealthdInternalsThermalCardElement;
  }
}

customElements.define(
    HealthdInternalsThermalCardElement.is, HealthdInternalsThermalCardElement);
