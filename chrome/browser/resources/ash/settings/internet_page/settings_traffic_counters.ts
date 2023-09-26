// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element to show traffic counters information in
 * Settings UI.
 */

import './internet_shared.css.js';
import 'chrome://resources/ash/common/network/network_shared.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/cr_elements/md_select.css.js';
import 'chrome://resources/ash/common/traffic_counters/traffic_counters.js';

import {Network, TrafficCountersAdapter} from 'chrome://resources/ash/common/traffic_counters/traffic_counters_adapter.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './settings_traffic_counters.html.js';

/**
 * Default day of the month, e.g., First of January, during
 * which traffic counters are reset.
 */
const DEFAULT_RESET_DAY = 1;

const KB = 1000;
const MB = KB * 1000;
const GB = MB * 1000;
const TB = GB * 1000;
const PB = TB * 1000;

/**
 * Returns a formatted string with the appropriate unit label and data size
 * fixed to two decimal values.
 */
function getDataInfoString(totalBytes: bigint): string {
  let unit: string = 'B';
  let dividend = 1;

  if (totalBytes >= PB) {
    unit = 'PB';
    dividend = PB;
  } else if (totalBytes >= TB) {
    unit = 'TB';
    dividend = TB;
  } else if (totalBytes >= GB) {
    unit = 'GB';
    dividend = GB;
  } else if (totalBytes >= MB) {
    unit = 'MB';
    dividend = MB;
  } else if (totalBytes >= KB) {
    unit = 'KB';
    dividend = KB;
  }

  const numBytes = (Number(totalBytes) / dividend).toFixed(2);
  return `${numBytes} ${unit}`;
}

const SettingsTrafficCountersElementBase = I18nMixin(PolymerElement);

export class SettingsTrafficCountersElement extends
    SettingsTrafficCountersElementBase {
  static get is() {
    return 'settings-traffic-counters' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** The network GUID to display details for. */
      guid: String,
      /** Tracks whether traffic counter info should be shown. */
      trafficCountersAvailable_: {
        type: Boolean,
        value: false,
      },
      /** Tracks the last reset time information. */
      date_: {
        type: String,
        value: '',
      },
      /** Tracks the traffic counter information. */
      value_: {
        type: String,
        value: '',
      },
      /** Tracks whether auto reset is enabled. */
      autoReset_: {
        type: Boolean,
        value: false,
      },
      /** Tracks the user specified day of reset. Default is 1. */
      resetDay_: {
        type: Number,
        value: 1,
      },
    };
  }

  guid: string;
  private autoReset_: boolean;
  private date_: string;
  private resetDay_: number;
  private trafficCountersAdapter_: TrafficCountersAdapter;
  private trafficCountersAvailable_: boolean;
  private value_: string;

  constructor() {
    super();

    /**
     * Adapter to collect network related information.
     */
    this.trafficCountersAdapter_ = new TrafficCountersAdapter();
    this.load();
  }

  /**
   * Loads all the values needed to populate the HTML.
   */
  load(): void {
    this.populateTrafficCountersAvailable_();
    this.populateDate_();
    this.populateDataUsageValue_();
    this.populateAutoResetValues_();
  }

  /**
   * Handles reset requests.
   */
  private async onResetDataUsageClick_(): Promise<void> {
    await this.trafficCountersAdapter_.resetTrafficCountersForNetwork(
        this.guid);
    this.load();
  }

  /**
   * Returns the network matching |this.guid| if it can be successfully
   * requested. Returns null otherwise.
   */
  private async getNetworkIfAvailable_(): Promise<Network|null> {
    const networks = await this.trafficCountersAdapter_
                         .requestTrafficCountersForActiveNetworks();
    const network = networks.find(n => n.guid === this.guid);
    return network || null;
  }

  /**
   * Determines whether data usage should be shown.
   */
  private async populateTrafficCountersAvailable_(): Promise<void> {
    const result = await this.populateTrafficCountersAvailableHelper_();
    this.trafficCountersAvailable_ = result;
  }

  /**
   * Gathers data usage visibility information for this network.
   */
  private async populateTrafficCountersAvailableHelper_(): Promise<boolean> {
    if (this.guid === '') {
      return false;
    }

    const network = await this.getNetworkIfAvailable_();
    return !!network;
  }

  /**
   * Determines the last reset time of the data usage.
   */
  private async populateDate_(): Promise<void> {
    const result = await this.populateDateHelper_();
    this.date_ = result;
  }

  /**
   * Gathers last reset time information.
   */
  private async populateDateHelper_(): Promise<string> {
    const network = await this.getNetworkIfAvailable_();
    if (network === null || network.friendlyDate === null) {
      return this.i18n('TrafficCountersDataUsageLastResetDateUnavailableLabel');
    }
    return this.i18n(
        'TrafficCountersDataUsageSinceLabel', network.friendlyDate);
  }

  /**
   * Determines the data usage value.
   */
  private async populateDataUsageValue_(): Promise<void> {
    const result = await this.populateDataUsageValueHelper_();
    this.value_ = result;
  }

  /**
   * Gathers the data usage value information.
   */
  private async populateDataUsageValueHelper_(): Promise<string> {
    const network = await this.getNetworkIfAvailable_();
    if (network === null) {
      return getDataInfoString(BigInt(0));
    }
    let totalBytes = BigInt(0);
    for (const sourceDict of network.counters) {
      totalBytes += BigInt(sourceDict.rxBytes) + BigInt(sourceDict.txBytes);
    }
    return getDataInfoString(totalBytes);
  }

  /**
   * Populates the auto reset enable and day values.
   */
  private populateAutoResetValues_(): void {
    this.populateEnableAutoResetBoolean_();
    this.populateUserSpecifiedResetDay_();
  }

  /**
   * Determines whether auto reset is enabled.
   */
  private async populateEnableAutoResetBoolean_(): Promise<void> {
    const result = await this.populateEnableAutoResetBooleanHelper_();
    this.autoReset_ = result;
  }

  /**
   * Gathers auto reset enable information.
   */
  private async populateEnableAutoResetBooleanHelper_(): Promise<boolean> {
    const network = await this.getNetworkIfAvailable_();
    return network ? network.autoReset : false;
  }

  /**
   * Determines the auto reset day.
   */
  private async populateUserSpecifiedResetDay_(): Promise<void> {
    const result = await this.populateUserSpecifiedResetDayHelper_();
    this.resetDay_ = result;
  }

  /**
   * Gathers the auto reset day information.
   */
  private async populateUserSpecifiedResetDayHelper_(): Promise<number> {
    const network = await this.getNetworkIfAvailable_();
    return network ? network.userSpecifiedResetDay : DEFAULT_RESET_DAY;
  }

  /**
   * Handles the auto reset toggle changes.
   */
  private onAutoDataUsageResetToggle_(): void {
    this.autoReset_ = !this.autoReset_;
    this.resetDay_ = 1;
    const day = this.autoReset_ ? {value: this.resetDay_} : null;
    this.trafficCountersAdapter_.setTrafficCountersAutoResetForNetwork(
        this.guid, this.autoReset_, day);
    this.load();
  }

  /**
   * Handles day of reset changes.
   */
  private onResetDaySelected_(): void {
    if (!this.autoReset_) {
      return;
    }
    this.trafficCountersAdapter_.setTrafficCountersAutoResetForNetwork(
        this.guid, this.autoReset_, {value: this.resetDay_});
    this.load();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsTrafficCountersElement.is]: SettingsTrafficCountersElement;
  }
}

customElements.define(
    SettingsTrafficCountersElement.is, SettingsTrafficCountersElement);
