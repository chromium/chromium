// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element to show traffic counters information in
 * Settings UI.
 */
import './internet_shared_css.js';
import 'chrome://resources/ash/common/network/network_shared.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/cr_elements/md_select.css.js';
import 'chrome://resources/ash/common/traffic_counters/traffic_counters.js';

import {Network, TrafficCountersAdapter} from 'chrome://resources/ash/common/traffic_counters/traffic_counters_adapter.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @type {number} Default day of the month, e.g., First of January, during
 * which traffic counters are reset.
 */
const kDefaultResetDay = 1;

const KB = 1000;
const MB = KB * 1000;
const GB = MB * 1000;
const TB = GB * 1000;
const PB = TB * 1000;

/**
 * Returns a formatted string with the appropriate unit label and data size
 * fixed to two decimal values.
 * @param {bigint} totalBytes
 * @return {string} the appropriate unit label
 */
function getDataInfoString(totalBytes) {
  let unit = 'B';
  let dividend = BigInt(1);

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

  return (parseFloat(totalBytes) / parseFloat(dividend)).toFixed(2) + ' ' +
      unit;
}

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const SettingsTrafficCountersElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class SettingsTrafficCountersElement extends
    SettingsTrafficCountersElementBase {
  static get is() {
    return 'settings-traffic-counters';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** The network GUID to display details for. */
      guid: String,
      /** Tracks whether traffic counter info should be shown. */
      trafficCountersAvailable_: {type: Boolean, value: false},
      /** Tracks the last reset time information. */
      date_: {type: String, value: ''},
      /** Tracks the traffic counter information. */
      value_: {type: String, value: ''},
      /** Tracks whether auto reset is enabled. */
      autoReset_: {type: Boolean, value: false},
      /** Tracks the user specified day of reset. Default is 1. */
      resetDay_: {type: Number, value: 1},
    };
  }

  constructor() {
    super();

    /**
     * Adapter to collect network related information.
     * @private {!TrafficCountersAdapter}
     */
    this.trafficCountersAdapter_ = new TrafficCountersAdapter();
    this.load();
  }

  /**
   * Loads all the values needed to populate the HTML.
   * @public
   */
  load() {
    this.populateTrafficCountersAvailable_();
    this.populateDate_();
    this.populateDataUsageValue_();
    this.populateAutoResetValues_();
  }

  /**
   * Handles reset requests.
   * @private
   */
  async onResetDataUsageClick_() {
    this.trafficCountersAdapter_.resetTrafficCountersForNetwork(this.guid);
    this.load();
  }

  /**
   * Returns the network matching |this.guid| if it can be successfully
   * requested. Returns null otherwise.
   * @return {!Promise<?Network>}
   */
  async getNetworkIfAvailable_() {
    const networks = await this.trafficCountersAdapter_
                         .requestTrafficCountersForActiveNetworks();
    const network = networks.find(n => n.guid === this.guid);
    return network === undefined ? null : network;
  }

  /**
   * Determines whether data usage should be shown.
   * @private
   */
  async populateTrafficCountersAvailable_() {
    const result = await this.populateTrafficCountersAvailableHelper_();
    this.trafficCountersAvailable_ = result;
  }

  /**
   * Gathers data usage visibility information for this network.
   * @return {!Promise<boolean>} Whether data usage should be shown.
   */
  async populateTrafficCountersAvailableHelper_() {
    if (this.guid === '') {
      return false;
    }
    return (await this.getNetworkIfAvailable_()) !== null ? true : false;
  }

  /**
   * Determines the last reset time of the data usage.
   * @private
   */
  async populateDate_() {
    const result = await this.populateDateHelper_();
    this.date_ = result;
  }

  /**
   * Gathers last reset time information.
   * @return {!Promise<string>} Date when data usage was last reset
   * @private
   */
  async populateDateHelper_() {
    const network = await this.getNetworkIfAvailable_();
    if (network === null || network.friendlyDate === null) {
      return this.i18n('TrafficCountersDataUsageLastResetDateUnavailableLabel');
    }
    return this.i18n(
        'TrafficCountersDataUsageSinceLabel', network.friendlyDate);
  }

  /**
   * Determines the data usage value.
   * @private
   */
  async populateDataUsageValue_() {
    const result = await this.populateDataUsageValueHelper_();
    this.value_ = result;
  }

  /**
   * Gathers the data usage value information.
   * @return {!Promise<string>} Value corresponding to the data usage
   * @private
   */
  async populateDataUsageValueHelper_() {
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
   * @private
   */
  populateAutoResetValues_() {
    this.populateEnableAutoResetBoolean_();
    this.populateUserSpecifiedResetDay_();
  }

  /**
   * Determines whether auto reset is enabled.
   * @private
   */
  async populateEnableAutoResetBoolean_() {
    const result = await this.populateEnableAutoResetBooleanHelper_();
    this.autoReset_ = result;
  }

  /**
   * Gathers auto reset enable information.
   * @return {!Promise<boolean>}
   * @private
   */
  async populateEnableAutoResetBooleanHelper_() {
    const network = await this.getNetworkIfAvailable_();
    return network !== null ? network.autoReset : false;
  }

  /**
   * Determines the auto reset day.
   * @private
   */
  async populateUserSpecifiedResetDay_() {
    const result = await this.populateUserSpecifiedResetDayHelper_();
    this.resetDay_ = result;
  }

  /**
   * Gathers the auto reset day information.
   * @return {!Promise<number>}
   * @private
   */
  async populateUserSpecifiedResetDayHelper_() {
    const network = await this.getNetworkIfAvailable_();
    return network !== null ? network.userSpecifiedResetDay : kDefaultResetDay;
  }

  /**
   * Handles the auto reset toggle changes.
   * @private
   */
  onAutoDataUsageResetToggle_() {
    this.autoReset_ = !this.autoReset_;
    this.resetDay_ = 1;
    const day = this.autoReset_ ? {value: this.resetDay_} : null;
    this.trafficCountersAdapter_.setTrafficCountersAutoResetForNetwork(
        this.guid, this.autoReset_, day);
    this.load();
  }

  /**
   * Handles day of reset changes.
   * @private
   */
  onResetDaySelected_() {
    if (!this.autoReset_) {
      return;
    }
    this.trafficCountersAdapter_.setTrafficCountersAutoResetForNetwork(
        this.guid, this.autoReset_, {value: this.resetDay_});
    this.load();
  }
}

customElements.define(
    SettingsTrafficCountersElement.is, SettingsTrafficCountersElement);
