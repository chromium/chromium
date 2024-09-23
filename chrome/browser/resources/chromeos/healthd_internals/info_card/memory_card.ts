// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './info_card.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {HealthdApiTelemetryResult} from '../externs.js';

import type {HealthdInternalsInfoCardElement} from './info_card.js';
import {getTemplate} from './memory_card.html.js';

export interface HealthdInternalsMemoryCardElement {
  $: {
    infoCard: HealthdInternalsInfoCardElement,
  };
}

export class HealthdInternalsMemoryCardElement extends PolymerElement {
  static get is() {
    return 'healthd-internals-memory-card';
  }

  static get template() {
    return getTemplate();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.$.infoCard.appendCardRow('INFO');
    this.$.infoCard.appendCardRow('SWAP');
    this.$.infoCard.appendCardRow('SLAB');
  }

  updateTelemetryData(data: HealthdApiTelemetryResult) {
    this.$.infoCard.updateDisplayedInfo(0, {
      'totalMemoryKib': data.memory.totalMemoryKib,
      'freeMemoryKib': data.memory.freeMemoryKib,
      'availableMemoryKib': data.memory.availableMemoryKib,
      'buffersKib': data.memory.buffersKib,
      'pageCacheKib': data.memory.pageCacheKib,
      'sharedMemoryKib': data.memory.sharedMemoryKib,
      'activeMemoryKib': data.memory.activeMemoryKib,
      'inactiveMemoryKib': data.memory.inactiveMemoryKib,
    });
    this.$.infoCard.updateDisplayedInfo(1, {
      'totalSwapMemoryKib': data.memory.totalSwapMemoryKib,
      'freeSwapMemoryKib': data.memory.freeSwapMemoryKib,
      'cachedSwapMemoryKib': data.memory.cachedSwapMemoryKib,
    });
    this.$.infoCard.updateDisplayedInfo(2, {
      'totalSlabMemoryKib': data.memory.totalSlabMemoryKib,
      'reclaimableSlabMemoryKib': data.memory.reclaimableSlabMemoryKib,
      'unreclaimableSlabMemoryKib': data.memory.unreclaimableSlabMemoryKib,
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-memory-card': HealthdInternalsMemoryCardElement;
  }
}

customElements.define(
    HealthdInternalsMemoryCardElement.is, HealthdInternalsMemoryCardElement);
