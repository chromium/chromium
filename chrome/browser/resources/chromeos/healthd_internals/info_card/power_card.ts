// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './info_card.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {HealthdApiTelemetryResult} from '../externs.js';

import type {HealthdInternalsInfoCardElement} from './info_card.js';
import {getTemplate} from './power_card.html.js';

export interface HealthdInternalsPowerCardElement {
  $: {
    infoCard: HealthdInternalsInfoCardElement,
  };
}

export class HealthdInternalsPowerCardElement extends PolymerElement {
  static get is() {
    return 'healthd-internals-power-card';
  }

  static get template() {
    return getTemplate();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.$.infoCard.appendCardRow('BATTERY');
  }

  updateTelemetryData(data: HealthdApiTelemetryResult) {
    this.$.infoCard.updateDisplayedInfo(0, data.battery);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-power-card':
        HealthdInternalsPowerCardElement;
  }
}

customElements.define(
    HealthdInternalsPowerCardElement.is,
    HealthdInternalsPowerCardElement);
