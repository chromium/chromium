// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './info_card.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {HealthdApiMemoryResult, HealthdApiTelemetryResult, SystemZramInfo} from '../externs.js';

import type {HealthdInternalsInfoCardElement} from './info_card.js';
import {getTemplate} from './memory_card.html.js';

/**
 * The value of memory unit in selected menu.
 */
enum MemoryUnitEnum {
  AUTO = 'auto',
  GIBI = 'gibibyte',
  MEBI = 'mebibyte',
  KIBI = 'kibibyte',
}

export interface HealthdInternalsMemoryCardElement {
  $: {
    infoCard: HealthdInternalsInfoCardElement,
    memoryUnitSelector: HTMLSelectElement,
  };
}

export class HealthdInternalsMemoryCardElement extends PolymerElement {
  static get is() {
    return 'healthd-internals-memory-card';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      memoryUnit: {type: String},
    };
  }

  override connectedCallback() {
    super.connectedCallback();

    this.$.infoCard.appendCardRow('INFO', true);
    this.$.infoCard.appendCardRow('SWAP', true);
    this.$.infoCard.appendCardRow('ZRAM', true);
    this.$.infoCard.appendCardRow('DETAILS');
  }

  // Displayed memory unit.
  private memoryUnit: MemoryUnitEnum = MemoryUnitEnum.AUTO;

  // The latest memory data to display.
  private latestMemoryInfo?: HealthdApiMemoryResult;
  private latestZramInfo?: SystemZramInfo;

  updateTelemetryData(data: HealthdApiTelemetryResult) {
    this.latestMemoryInfo = data.memory;
    this.refreshMemoryCard();
  }

  updateZramData(zram: SystemZramInfo) {
    this.latestZramInfo = zram;
    this.refreshMemoryCard();
  }

  updateExpanded(isExpanded: boolean) {
    this.$.infoCard.updateExpanded(isExpanded);
  }

  private refreshMemoryCard() {
    if (this.latestMemoryInfo !== undefined) {
      const memory = this.latestMemoryInfo;
      this.updateInfoRow(
          parseInt(memory.totalMemoryKib), parseInt(memory.freeMemoryKib),
          parseInt(memory.availableMemoryKib));
      if (memory.totalSwapMemoryKib !== undefined &&
          memory.freeSwapMemoryKib !== undefined) {
        this.updateSwapRow(
            parseInt(memory.totalSwapMemoryKib),
            parseInt(memory.freeSwapMemoryKib));
      }
      this.updateDetailsRow(memory);
    }
    if (this.latestZramInfo !== undefined) {
      this.updateZramRow(this.latestZramInfo)
    }
  }

  private updateInfoRow(
      totalMemoryKib: number, freeMemoryKib: number,
      availableMemoryKib: number) {
    const usedMemoryKib = totalMemoryKib - availableMemoryKib;
    this.$.infoCard.updateDisplayedInfo(0, {
      'Total': this.getFormattedMemory(totalMemoryKib),
      'Used':
          this.getFormattedMemoryWithPercentage(usedMemoryKib, totalMemoryKib),
      'Avail': this.getFormattedMemoryWithPercentage(
          availableMemoryKib, totalMemoryKib),
      'Free':
          this.getFormattedMemoryWithPercentage(freeMemoryKib, totalMemoryKib),
    });
  }

  private updateSwapRow(totalSwapMemoryKib: number, freeSwapMemoryKib: number) {
    const usedSwapMemoryKib = totalSwapMemoryKib - freeSwapMemoryKib;
    this.$.infoCard.updateDisplayedInfo(1, {
      'Total': this.getFormattedMemory(totalSwapMemoryKib),
      'Used': this.getFormattedMemoryWithPercentage(
          usedSwapMemoryKib, totalSwapMemoryKib),
      'Free': this.getFormattedMemoryWithPercentage(
          freeSwapMemoryKib, totalSwapMemoryKib),
    });
  }

  private updateZramRow(zram: SystemZramInfo) {
    const totalUsedMemoryKib = parseInt(zram.totalUsedMemory) / 1024;
    const originalDataSizeKib = parseInt(zram.originalDataSize) / 1024;
    const compressedDataSizeKib = parseInt(zram.compressedDataSize) / 1024;

    // In general, higher compression ratio means better compression.
    const compressionRatio = originalDataSizeKib / compressedDataSizeKib;
    const spaceReductionPercentage =
        (originalDataSizeKib - compressedDataSizeKib) / originalDataSizeKib *
        100;

    this.$.infoCard.updateDisplayedInfo(2, {
      'Total Used': this.getFormattedMemory(totalUsedMemoryKib),
      'Original Size': this.getFormattedMemory(originalDataSizeKib),
      'Compressed Size': this.getFormattedMemory(compressedDataSizeKib),
      'Compression Ratio': compressionRatio.toFixed(2),
      'Space Reduction': `${spaceReductionPercentage.toFixed(2)}%`,
    });
  }

  private updateDetailsRow(memory: HealthdApiMemoryResult) {
    this.$.infoCard.updateDisplayedInfo(3, {
      'Buffers': this.getFormattedMemoryFromRaw(memory.buffersKib),
      'Page Cache': this.getFormattedMemoryFromRaw(memory.pageCacheKib),
      'Shared': this.getFormattedMemoryFromRaw(memory.sharedMemoryKib),
      'Active': this.getFormattedMemoryFromRaw(memory.activeMemoryKib),
      'Inactive': this.getFormattedMemoryFromRaw(memory.inactiveMemoryKib),
      'Total Slab': this.getFormattedMemoryFromRaw(memory.totalSlabMemoryKib),
      'Reclaimable Slab':
          this.getFormattedMemoryFromRaw(memory.reclaimableSlabMemoryKib),
      'Unreclaimable Slab':
          this.getFormattedMemoryFromRaw(memory.unreclaimableSlabMemoryKib),
      'Cached Swap': this.getFormattedMemoryFromRaw(memory.cachedSwapMemoryKib),
    });
  }

  private getFormattedMemoryFromRaw(rawMemoryKiB?: string): string {
    if (rawMemoryKiB === undefined) {
      return 'N/A';
    }
    return this.getFormattedMemory(parseInt(rawMemoryKiB));
  }

  private getFormattedMemoryWithPercentage(memory: number, totalMemory: number):
      string {
    return `${this.getFormattedMemory(memory)} (${
        (memory / totalMemory * 100).toFixed(2)}%)`;
  }

  private getFormattedMemory(memory: number): string {
    switch (this.memoryUnit) {
      case MemoryUnitEnum.AUTO: {
        const units = ['KiB', 'MiB', 'GiB'];
        let unitIdx = 0;
        while (memory > 1024 && unitIdx + 1 < units.length) {
          memory /= 1024;
          unitIdx++;
        }
        return `${memory.toFixed(2)} ${units[unitIdx]}`;
      }
      case MemoryUnitEnum.GIBI: {
        return `${(memory / 1024 / 1024).toFixed(2)} GiB`;
      }
      case MemoryUnitEnum.MEBI: {
        return `${(memory / 1024).toFixed(2)} MiB`;
      }
      case MemoryUnitEnum.KIBI: {
        return `${memory} KiB`;
      }
      default: {
        console.error('Unknown memory unit: ', this.memoryUnit);
        return 'N/A';
      }
    }
  }

  private onMemoryUnitChanged() {
    this.memoryUnit = this.$.memoryUnitSelector.value as MemoryUnitEnum;
    this.refreshMemoryCard();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-memory-card': HealthdInternalsMemoryCardElement;
  }
}

customElements.define(
    HealthdInternalsMemoryCardElement.is, HealthdInternalsMemoryCardElement);
