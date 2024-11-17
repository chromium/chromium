// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './info_card.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {HealthdApiTelemetryResult} from '../externs.js';

import {getTemplate} from './fan_card.html.js';
import type {HealthdInternalsInfoCardElement} from './info_card.js';

export interface HealthdInternalsFanCardElement {
  $: {
    infoCard: HealthdInternalsInfoCardElement,
  };
}

export class HealthdInternalsFanCardElement extends PolymerElement {
  static get is() {
    return 'healthd-internals-fan-card';
  }

  static get template() {
    return getTemplate();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.$.infoCard.appendCardRow('SPEED (RPM)');
    this.$.infoCard.updateDisplayedInfo(0, 'Fans not found.');
  }

  updateTelemetryData(data: HealthdApiTelemetryResult) {
    if (data.fans.length === 0) {
      return;
    }
    const speedInfo = data.fans.reduce((acc, fan, index) => {
      acc[`Fan #${index}`] = parseInt(fan.speedRpm);
      return acc;
    }, {} as Record<string, number>);
    this.$.infoCard.updateDisplayedInfo(0, speedInfo);
  }

  updateExpanded(isExpanded: boolean) {
    this.$.infoCard.updateExpanded(isExpanded);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-fan-card': HealthdInternalsFanCardElement;
  }
}

customElements.define(
    HealthdInternalsFanCardElement.is, HealthdInternalsFanCardElement);
