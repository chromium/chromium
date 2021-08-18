// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Item in <os-paired_bluetooth-list> that displays information for a paired
 * Bluetooth device.
 */

import '../../settings_shared_css.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import '../os_icons.m.js';

import {I18nBehavior, I18nBehaviorInterface} from '//resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getDeviceName} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_utils.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {FocusRowBehavior, FocusRowBehaviorInterface} from 'chrome://resources/js/cr/ui/focus_row_behavior.m.js';

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
 * @implements {FocusRowBehaviorInterface}
 */
const SettingsPairedBluetoothListItemElementBase =
    mixinBehaviors([I18nBehavior, FocusRowBehavior], PolymerElement);

/** @polymer */
class SettingsPairedBluetoothListItemElement extends
    SettingsPairedBluetoothListItemElementBase {
  static get is() {
    return 'os-settings-paired-bluetooth-list-item';
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

      /** The index of this item in its parent list, used for its a11y label. */
      itemIndex: Number,

      /**
       * The total number of elements in this item's parent list, used for its
       * a11y label.
       */
      listSize: Number,

      /** @private {boolean} */
      isBatteryPercentageAvailable_: {
        type: Boolean,
        computed: 'computeIsBatteryPercentageAvailable_(device)',
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
   * @return {string}
   * @private
   */
  getDeviceName_(device) {
    return getDeviceName(device);
  }

  /**
   * @param {!chromeos.bluetoothConfig.mojom.PairedBluetoothDeviceProperties}
   *     device
   * @return {string}
   * @private
   */
  getDeviceTypeIcon_(device) {
    const deviceType = chromeos.bluetoothConfig.mojom.DeviceType;
    switch (device.deviceProperties.deviceType) {
      case deviceType.kComputer:
        return 'bluetooth-computer';
      case deviceType.kPhone:
        return 'bluetooth-phone';
      case deviceType.kHeadset:
        return 'bluetooth-headset';
      case deviceType.kVideoCamera:
        return 'bluetooth-video-camera';
      case deviceType.kGameController:
        return 'bluetooth-game-controller';
      case deviceType.kKeyboard:
        return 'bluetooth-keyboard';
      case deviceType.kMouse:
        return 'bluetooth-mouse';
      case deviceType.kTablet:
        return 'bluetooth-tablet';
      default:
        return 'bluetooth';
    }
  }

  /**
   * Returns the battery percentage of device, or undefined if device does
   * not exist or it has no battery information. Clients that call this method
   * should explicitly check if the return value is undefined to differentiate
   * it from a return value of 0.
   * @param {!chromeos.bluetoothConfig.mojom.PairedBluetoothDeviceProperties}
   *     device
   * @return {number|undefined}
   * @private
   */
  getBatteryPercentage_(device) {
    if (!device) {
      return undefined;
    }

    const batteryInfo = device.deviceProperties.batteryInfo;
    if (!batteryInfo || !batteryInfo.defaultProperties) {
      return undefined;
    }

    return batteryInfo.defaultProperties.batteryPercentage;
  }

  /**
   * @param {!chromeos.bluetoothConfig.mojom.PairedBluetoothDeviceProperties}
   *     device
   * @return {boolean}
   * @private
   */
  computeIsBatteryPercentageAvailable_(device) {
    const batteryPercentage = this.getBatteryPercentage_(device);
    if (batteryPercentage === undefined) {
      return false;
    }
    return batteryPercentage >= 0 && batteryPercentage <= 100;
  }

  /**
   * @param {!chromeos.bluetoothConfig.mojom.PairedBluetoothDeviceProperties}
   *     device
   * @return {boolean}
   * @private
   */
  computeIsLowBattery_(device) {
    if (!this.isBatteryPercentageAvailable_) {
      return false;
    }
    const batteryPercentage = this.getBatteryPercentage_(device);
    return batteryPercentage < LOW_BATTERY_THRESHOLD_PERCENTAGE;
  }

  /**
   * @param {!chromeos.bluetoothConfig.mojom.PairedBluetoothDeviceProperties}
   *     device
   * @return {string}
   * @private
   */
  getBatteryPercentageString_(device) {
    if (!this.isBatteryPercentageAvailable_) {
      return '';
    }
    const batteryPercentage = this.getBatteryPercentage_(device);

    // Required for closure compiler not to complain about the line below.
    assert(batteryPercentage !== undefined);
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
    if (!this.isBatteryPercentageAvailable_) {
      return '';
    }
    const batteryPercentage = this.getBatteryPercentage_(device);

    // Range should always find a value because isBatteryPercentageAvailable_
    // ensures batteryPercentage is within bounds.
    const range = BATTERY_ICONS_RANGES.find(range => {
      return range[0] <= batteryPercentage && batteryPercentage <= range[1];
    });
    assert(
        !!range && range.length === 2, 'Battery percentage range is invalid');

    return 'os-settings:battery-' + range[0] + '-' + range[1];
  }

  /**
   * @param {!chromeos.bluetoothConfig.mojom.PairedBluetoothDeviceProperties}
   *     device
   * @return {string}
   * @private
   */
  getAriaLabel_(device) {
    const deviceName = this.getDeviceName_(device);
    const deviceType = chromeos.bluetoothConfig.mojom.DeviceType;
    let stringName;
    switch (device.deviceProperties.deviceType) {
      case deviceType.kComputer:
        stringName = this.isBatteryPercentageAvailable_ ?
            'bluetoothPairedDeviceItemA11yLabelTypeComputerWithBatteryInfo' :
            'bluetoothPairedDeviceItemA11yLabelTypeComputer';
        break;
      case deviceType.kPhone:
        stringName = this.isBatteryPercentageAvailable_ ?
            'bluetoothPairedDeviceItemA11yLabelTypePhoneWithBatteryInfo' :
            'bluetoothPairedDeviceItemA11yLabelTypePhone';
        break;
      case deviceType.kHeadset:
        stringName = this.isBatteryPercentageAvailable_ ?
            'bluetoothPairedDeviceItemA11yLabelTypeHeadsetWithBatteryInfo' :
            'bluetoothPairedDeviceItemA11yLabelTypeHeadset';
        break;
      case deviceType.kVideoCamera:
        stringName = this.isBatteryPercentageAvailable_ ?
            'bluetoothPairedDeviceItemA11yLabelTypeVideoCameraWithBatteryInfo' :
            'bluetoothPairedDeviceItemA11yLabelTypeVideoCamera';
        break;
      case deviceType.kGameController:
        stringName = this.isBatteryPercentageAvailable_ ?
            'bluetoothPairedDeviceItemA11yLabelTypeGameControllerWithBatteryInfo' :
            'bluetoothPairedDeviceItemA11yLabelTypeGameController';
        break;
      case deviceType.kKeyboard:
        stringName = this.isBatteryPercentageAvailable_ ?
            'bluetoothPairedDeviceItemA11yLabelTypeKeyboardWithBatteryInfo' :
            'bluetoothPairedDeviceItemA11yLabelTypeKeyboard';
        break;
      case deviceType.kMouse:
        stringName = this.isBatteryPercentageAvailable_ ?
            'bluetoothPairedDeviceItemA11yLabelTypeMouseWithBatteryInfo' :
            'bluetoothPairedDeviceItemA11yLabelTypeMouse';
        break;
      case deviceType.kTablet:
        stringName = this.isBatteryPercentageAvailable_ ?
            'bluetoothPairedDeviceItemA11yLabelTypeTabletWithBatteryInfo' :
            'bluetoothPairedDeviceItemA11yLabelTypeTablet';
        break;
      default:
        stringName = this.isBatteryPercentageAvailable_ ?
            'bluetoothPairedDeviceItemA11yLabelTypeUnknownWithBatteryInfo' :
            'bluetoothPairedDeviceItemA11yLabelTypeUnknown';
    }

    if (!this.isBatteryPercentageAvailable_) {
      return this.i18n(
          stringName, this.itemIndex + 1, this.listSize, deviceName);
    }

    const batteryPercentage = this.getBatteryPercentage_(device);
    assert(batteryPercentage !== undefined);
    return this.i18n(
        stringName, this.itemIndex + 1, this.listSize, deviceName,
        batteryPercentage);
  }

  /**
   * @param {!chromeos.bluetoothConfig.mojom.PairedBluetoothDeviceProperties}
   *     device
   * @return {string}
   * @private
   */
  getSubpageButtonA11yLabel_(device) {
    const deviceName = this.getDeviceName_(device);
    return this.i18n(
        'bluetoothPairedDeviceItemSubpageButtonA11yLabel', deviceName);
  }
}

customElements.define(
    SettingsPairedBluetoothListItemElement.is,
    SettingsPairedBluetoothListItemElement);