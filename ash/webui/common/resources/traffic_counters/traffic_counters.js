// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/network/network_shared.css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {TrafficCounterSource} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {Time} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './traffic_counters.html.js';
import {TrafficCountersAdapter} from './traffic_counters_adapter.js';

/**
 * @fileoverview Polymer element for a container used in displaying network
 * traffic information.
 */

/**
 * Image file name corresponding to network type.
 * @enum {string}
 */
const TechnologyIcons = {
  CELLULAR: 'cellular_0.svg',
  ETHERNET: 'ethernet.svg',
  VPN: 'vpn.svg',
  WIFI: 'wifi_0.svg',
};

/**
 * Information about a network.
 * @typedef {{
 *   guid: string,
 *   name: string,
 *   type: !NetworkType,
 *   counters: !Array<!Object>,
 *   lastResetTime: ?Time,
 * }}
 */
let Network;

/**
 * Helper function to create a Network object.
 * @param {string} guid
 * @param {string} name
 * @param {!NetworkType} type
 * @param {!Array<!Object>} counters
 * @param {?Time} lastResetTime
 * @return {Network} Network object
 */
function createNetwork(guid, name, type, counters, lastResetTime) {
  return {
    guid: guid,
    name: name,
    type: type,
    counters: counters,
    lastResetTime: lastResetTime,
  };
}

/**
 * Replacer function used to handle the bigint type.
 * @param {string} key
 * @param {*|null|undefined} value
 * @return {*|undefined} value in string format
 */
function replacer(key, value) {
  return typeof value === 'bigint' ? value.toString() : value;
}

/**
 * Converts a mojo time to JS. TODO(b/200327630)
 * @param {!Time} mojoTime
 * @return {!Date}
 */
function convertMojoTimeToJS(mojoTime) {
  // The JS Date() is based off of the number of milliseconds since the
  // UNIX epoch (1970-01-01 00::00:00 UTC), while |internalValue| of the
  // base::Time (represented in mojom.Time) represents the number of
  // microseconds since the Windows FILETIME epoch (1601-01-01 00:00:00 UTC).
  // This computes the final JS time by computing the epoch delta and the
  // conversion from microseconds to milliseconds.
  const windowsEpoch = Date.UTC(1601, 0, 1, 0, 0, 0, 0);
  const unixEpoch = Date.UTC(1970, 0, 1, 0, 0, 0, 0);
  // |epochDeltaInMs| equals to base::Time::kTimeToMicrosecondsOffset.
  const epochDeltaInMs = unixEpoch - windowsEpoch;
  const timeInMs = Number(mojoTime.internalValue) / 1000;

  return new Date(timeInMs - epochDeltaInMs);
}


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const TrafficCountersElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class TrafficCountersElement extends TrafficCountersElementBase {
  static get is() {
    return 'traffic-counters';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Information about networks.
       * @private {!Array<!Network>}
       */
      networks_: {type: Array, value: []},
    };
  }

  constructor() {
    super();

    /**
     * Expanded state per network type.
     * @private {!Array<boolean>}
     */
    this.typeExpanded_ = [];

    /**
     * Adapter to access traffic counters functionality.
     * @private {!TrafficCountersAdapter}
     */
    this.trafficCountersAdapter_ = new TrafficCountersAdapter();
  }

  /**
   * Handles requests to request traffic counters.
   * @private
   */
  async onRequestTrafficCountersClick_() {
    await this.fetchTrafficCountersForActiveNetworks_();
  }

  /**
   * Handles requests to reset traffic counters.
   * @param {!Event} event
   * @private
   */
  async onResetTrafficCountersClick_(event) {
    const network = event.model.network;
    await this.trafficCountersAdapter_.resetTrafficCountersForNetwork(
        network.guid);
    let trafficCounters =
        await this.trafficCountersAdapter_.requestTrafficCountersForNetwork(
            network.guid);
    trafficCounters = this.convertSourceEnumToString_(trafficCounters);
    const lastResetTime =
        await this.trafficCountersAdapter_.requestLastResetTimeForNetwork(
            network.guid);
    const foundIdx = this.networks_.findIndex(n => n.guid === network.guid);
    if (foundIdx === -1) {
      return;
    }
    this.splice(
        'networks_', foundIdx, 1,
        createNetwork(
            network.guid, network.name, network.type, trafficCounters,
            lastResetTime));
  }

  /**
   * Requests traffic counters for networks.
   * @return {!Promise<!Array<!Network>>} information about networks
   */
  async fetchTrafficCountersForActiveNetworks_() {
    const networks = await this.trafficCountersAdapter_
                         .requestTrafficCountersForActiveNetworks();
    for (const network of networks) {
      network.counters = this.convertSourceEnumToString_(network.counters);
    }
    this.networks_ = networks;
    return this.networks_;
  }

  /**
   * @param {!NetworkType} type
   * @return {string} A string for the given NetworkType.
   * @private
   */
  getNetworkTypeString_(type) {
    return this.i18n('OncType' + OncMojo.getNetworkTypeString(type));
  }

  /**
   * @param {!NetworkType} type
   * @return {string} An icon for the given NetworkType.
   * @private
   */
  getNetworkTypeIcon_(type) {
    switch (type) {
      case NetworkType.kEthernet:
        return TechnologyIcons.ETHERNET;
      case NetworkType.kWiFi:
        return TechnologyIcons.WIFI;
      case NetworkType.kVPN:
        return TechnologyIcons.VPN;
      case NetworkType.kTether:
      case NetworkType.kMobile:
      case NetworkType.kCellular:
        return TechnologyIcons.CELLULAR;
      default:
        return '';
    }
  }

  /**
   * @param {!NetworkType} type
   * @return {boolean} Whether type should be expanded.
   * @private
   * */
  getTypeExpanded_(type) {
    if (this.typeExpanded_[type] === undefined) {
      this.set('typeExpanded_.' + type, false);
      return false;
    }
    return this.typeExpanded_[type];
  }

  /**
   * Helper function to toggle the expanded properties when the network
   * container is toggled.
   * @param {!Event} event
   * @private
   */
  onToggleExpanded_(event) {
    const type = event.model.network.type;
    this.set('typeExpanded_.' + type, !this.typeExpanded_[type]);
  }

  /**
   * Convert the traffic counters' source enum to a readable string.
   * @param {!Array<!Object>} counters with source enum
   * @return {!Array<!Object>} counters with source string
   * @private
   */
  convertSourceEnumToString_(counters) {
    for (const counter of counters) {
      switch (counter.source) {
        case TrafficCounterSource.kUnknown:
          counter.source = this.i18n('TrafficCountersUnknown');
          break;
        case TrafficCounterSource.kChrome:
          counter.source = this.i18n('TrafficCountersChrome');
          break;
        case TrafficCounterSource.kUser:
          counter.source = this.i18n('TrafficCountersUser');
          break;
        case TrafficCounterSource.kArc:
          counter.source = this.i18n('TrafficCountersArc');
          break;
        case TrafficCounterSource.kCrosvm:
          counter.source = this.i18n('TrafficCountersCrosvm');
          break;
        case TrafficCounterSource.kPluginvm:
          counter.source = this.i18n('TrafficCountersPluginvm');
          break;
        case TrafficCounterSource.kUpdateEngine:
          counter.source = this.i18n('TrafficCountersUpdateEngine');
          break;
        case TrafficCounterSource.kVpn:
          counter.source = this.i18n('TrafficCountersVpn');
          break;
        case TrafficCounterSource.kSystem:
          counter.source = this.i18n('TrafficCountersSystem');
      }
    }
    return counters;
  }

  /**
   * @param {!Array<!Object>} counters
   * @return {string} A string representation of the traffic counters for a
   *     particular network.
   * @private
   */
  countersToString_(counters) {
    // '\t' describes the number of white space characters to use as white space
    // while forming the JSON string.
    return JSON.stringify(counters, replacer, '\t');
  }

  /**
   * @param {Network} network
   * @return {string} a representation of the last reset time for a particular
   * network.
   * @private
   */
  lastResetTimeString_(network) {
    if (network.lastResetTime === null || network.lastResetTime === undefined) {
      return '';
    }
    return convertMojoTimeToJS(network.lastResetTime).toLocaleString();
  }
}
customElements.define(TrafficCountersElement.is, TrafficCountersElement);
