// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * View displaying Bluetooth device battery information.
 */

import '../../settings_shared_css.js';
import '../os_icons.m.js';

import {I18nBehavior, I18nBehaviorInterface} from '//resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getBatteryPercentage} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_utils.js';
import {assert} from 'chrome://resources/js/assert.m.js';

/**
 * The threshold percentage where any battery percentage lower is considered
 * 'low battery'.
 * @type {number}
 */
const LOW_BATTERY_THRESHOLD_PERCENTAGE = 25;

/**
 * Ranges for each battery icon, where the value of the first index is the
 * minimum battery percentage in the range (inclusive), and the second index is
 * the maximum battery percentage in the range (inclusive).
 * @type {Array<Array<number>>}
 */
const BATTERY_ICONS_RANGES = [
  [0, 7], [8, 14], [15, 21], [22, 28], [29, 35], [36, 42], [43, 49], [50, 56],
  [57, 63], [64, 70], [71, 77], [78, 85], [86, 92], [93, 100]
];

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const SettingsBluetoothDeviceBatteryInfoElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class SettingsBluetoothDeviceBatteryInfoElement extends
    SettingsBluetoothDeviceBatteryInfoElementBase {
  static get is() {
    return 'os-settings-bluetooth-device-battery-info';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @private {!chromeos.bluetoothConfig.mojom.PairedBluetoothDeviceProperties}
       */
      device: {
        type: Object,
      },

      /** @private {boolean} */
      isLowBattery_: {
        reflectToAttribute: true,
        type: Boolean,
        computed: 'computeIsLowBattery_(device)',
      }
    };
  }

  /**
   * @param {!chromeos.bluetoothConfig.mojom.PairedBluetoothDeviceProperties}
   *     device
   * @return {boolean}
   * @private
   */
  computeIsLowBattery_(device) {
    const batteryPercentage = getBatteryPercentage(device);
    if (batteryPercentage === undefined) {
      return false;
    }
    return batteryPercentage < LOW_BATTERY_THRESHOLD_PERCENTAGE;
  }

  /**
   * @param {!chromeos.bluetoothConfig.mojom.PairedBluetoothDeviceProperties}
   *     device
   * @return {string}
   * @private
   */
  getBatteryPercentageString_(device) {
    const batteryPercentage = getBatteryPercentage(device);
    if (batteryPercentage === undefined) {
      return '';
    }

    return this.i18n(
        'bluetoothPairedDeviceItemBatteryPercentage', batteryPercentage);
  }

  /**
   * @param {!chromeos.bluetoothConfig.mojom.PairedBluetoothDeviceProperties}
   *     device
   * @return {string}
   * @private
   */
  getBatteryIcon_(device) {
    const batteryPercentage = getBatteryPercentage(device);
    if (batteryPercentage === undefined) {
      return '';
    }

    // Range should always find a value because this element should not be
    // showing if batteryPercentage is out of bounds.
    const range = BATTERY_ICONS_RANGES.find(range => {
      return range[0] <= batteryPercentage && batteryPercentage <= range[1];
    });
    assert(
        !!range && range.length === 2, 'Battery percentage range is invalid');

    return 'os-settings:battery-' + range[0] + '-' + range[1];
  }
}

customElements.define(
    SettingsBluetoothDeviceBatteryInfoElement.is,
    SettingsBluetoothDeviceBatteryInfoElement);