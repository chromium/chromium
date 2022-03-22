// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * View displaying Bluetooth device True Wireless Images and battery
 * information.
 */

import '../../settings_shared_css.js';
import 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_battery_icon_percentage.js';

import {assertNotReached} from '//resources/js/assert.m.js';
import {I18nBehavior, I18nBehaviorInterface} from '//resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {BatteryType} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_types.js';
import {getBatteryPercentage, hasAnyDetailedBatteryInfo, hasDefaultImage, hasTrueWirelessImages} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_utils.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const SettingsBluetoothTrueWirelessImagesElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class SettingsBluetoothTrueWirelessImagesElement extends
    SettingsBluetoothTrueWirelessImagesElementBase {
  static get is() {
    return 'os-settings-bluetooth-true-wireless-images';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @type {!chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties}
       */
      device: {
        type: Object,
      },

      /**
       * Enum used as an ID for specific UI elements.
       * A BatteryType is passed between html and JS for
       * certain UI elements to determine their state.
       *
       * @type {!BatteryType}
       */
      BatteryType: {
        type: Object,
        value: BatteryType,
      },
    };
  }

  /**
   * Only show specific battery information if the device is
   * Connected and there exists information for that device.
   *
   * @param {!chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties}
   *     device
   * @param {!BatteryType} batteryType
   * @return {boolean}
   * @protected
   */
  shouldShowBatteryTypeInfo_(device, batteryType) {
    if (device.connectionState !==
            chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected ||
        !hasTrueWirelessImages(device)) {
      return false;
    }

    return getBatteryPercentage(device, batteryType) !== undefined;
  }

  /**
   * If the device is connected but we don't have detailed battery information
   * for that device, fallback to showing the default image with default info.
   * We also display the default image alongside the "Disconnected" label when
   * the device is disconnected.
   *
   * @param {!chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties}
   *     device
   * @return {boolean}
   * @protected
   */
  shouldShowDefaultInfo_(device) {
    if (!hasDefaultImage(device)) {
      return false;
    }

    // Always show the default image when the device is not connected.
    if (this.isDeviceNotConnected_(device)) {
      return true;
    }

    // If we aren't showing any other detailed battery info and we have
    // default battery info, show the default image alongside the default info.
    return !hasAnyDetailedBatteryInfo(device) &&
        getBatteryPercentage(device, BatteryType.DEFAULT) !== undefined;
  }


  /**
   * @param {!chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties}
   *     device
   * @return {boolean}
   * @protected
   */
  isDeviceNotConnected_(device) {
    return device.connectionState ===
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kNotConnected ||
        device.connectionState ===
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnecting;
  }

  /**
   * @param {!chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties}
   *     device
   * @param {!BatteryType} batteryType
   * @return {string}
   * @protected
   */
  getBatteryTypeString_(device, batteryType) {
    switch (batteryType) {
      case BatteryType.LEFT_BUD:
        return this.i18n('bluetoothTrueWirelessLeftBudLabel');
      case BatteryType.CASE:
        return this.i18n('bluetoothTrueWirelessCaseLabel');
      case BatteryType.RIGHT_BUD:
        return this.i18n('bluetoothTrueWirelessRightBudLabel');
      default:
        assertNotReached();
    }
  }

  /**
   * @param {!chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties}
   *     device
   * @param {!BatteryType} batteryType
   * @return {string}
   * @private
   */
  getImageSrc_(device, batteryType) {
    switch (batteryType) {
      case BatteryType.DEFAULT:
        return this.device.imageInfo.defaultImageUrl.url;
      case BatteryType.LEFT_BUD:
        return this.device.imageInfo.trueWirelessImages.leftBudImageUrl.url;
      case BatteryType.CASE:
        return this.device.imageInfo.trueWirelessImages.caseImageUrl.url;
      case BatteryType.RIGHT_BUD:
        return this.device.imageInfo.trueWirelessImages.rightBudImageUrl.url;
      default:
        assertNotReached();
    }
  }
}

customElements.define(
    SettingsBluetoothTrueWirelessImagesElement.is,
    SettingsBluetoothTrueWirelessImagesElement);
