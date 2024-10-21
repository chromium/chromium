// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './info_card.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {HealthdApiTelemetryResult, HealthdApiThermalResult} from '../externs.js';

import type {HealthdInternalsInfoCardElement} from './info_card.js';
import {getTemplate} from './thermal_card.html.js';

/**
 * The value of temperature unit in selected menu.
 */
enum TemperatureUnitEnum {
  CELSIUS = 'celsius',
  FAHRENHEIT = 'fahrenheit',
}


export interface HealthdInternalsThermalCardElement {
  $: {
    infoCard: HealthdInternalsInfoCardElement,
    temperatureUnitSelector: HTMLSelectElement,
  };
}

export class HealthdInternalsThermalCardElement extends PolymerElement {
  static get is() {
    return 'healthd-internals-thermal-card';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      temperatureUnit: {type: String},
    };
  }

  // Displayed temperature unit.
  private temperatureUnit: TemperatureUnitEnum = TemperatureUnitEnum.CELSIUS;

  // The latest telemetry data to display thermal info.
  private latestTelemetryData?: HealthdApiTelemetryResult;

  override connectedCallback() {
    super.connectedCallback();

    this.$.infoCard.appendCardRow('EC');
    this.$.infoCard.appendCardRow('SYSFS (thermal_zone)');
    this.$.infoCard.appendCardRow('CPU TEMP CHANNELS');
  }

  updateTelemetryData(data: HealthdApiTelemetryResult) {
    this.latestTelemetryData = data;
    this.refreshThermalCard();
  }

  updateExpanded(isExpanded: boolean) {
    this.$.infoCard.updateExpanded(isExpanded);
  }

  private refreshThermalCard() {
    if (this.latestTelemetryData === undefined) {
      return
    }
    const data = this.latestTelemetryData;
    this.$.infoCard.updateDisplayedInfo(
        0, this.filterThermalsBySource(data.thermals, 'EC'));
    this.$.infoCard.updateDisplayedInfo(
        1, this.filterThermalsBySource(data.thermals, 'SysFs'));
    const cpuTemperature = data.cpu.temperatureChannels.map(
        item => ({
          'Label': item.label,
          'Temperature': this.convertTemperature(item.temperatureCelsius),
        }));
    this.$.infoCard.updateDisplayedInfo(2, cpuTemperature);
  }

  private filterThermalsBySource(
      thermals: HealthdApiThermalResult[], targetSource: string) {
    return thermals.filter(item => item.source === targetSource)
        .map(item => ({
               'Name': item.name,
               'Temperature': this.convertTemperature(item.temperatureCelsius),
             }));
  }

  private convertTemperature(temperatureCelsius: number): string {
    switch (this.temperatureUnit) {
      case TemperatureUnitEnum.CELSIUS: {
        return `${temperatureCelsius.toFixed(4)} °C`;
      }
      case TemperatureUnitEnum.FAHRENHEIT: {
        return `${(temperatureCelsius * 9 / 5 + 32).toFixed(4)} °F`;
      }
      default: {
        console.error('Unknown temperature unit: ', this.temperatureUnit);
        return 'N/A';
      }
    }
  }

  private onTemperatureUnitChanged() {
    this.temperatureUnit =
        this.$.temperatureUnitSelector.value as TemperatureUnitEnum;
    this.refreshThermalCard();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-thermal-card': HealthdInternalsThermalCardElement;
  }
}

customElements.define(
    HealthdInternalsThermalCardElement.is, HealthdInternalsThermalCardElement);
