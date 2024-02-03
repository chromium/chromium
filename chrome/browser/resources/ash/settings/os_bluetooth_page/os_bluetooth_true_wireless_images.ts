// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * View displaying Bluetooth device True Wireless Images and battery
 * information.
 */

import '../settings_shared.css.js';
import 'chrome://resources/ash/common/bluetooth/bluetooth_battery_icon_percentage.js';

import {BatteryType} from 'chrome://resources/ash/common/bluetooth/bluetooth_types.js';
import {getBatteryPercentage, hasAnyDetailedBatteryInfo, hasDefaultImage, hasTrueWirelessImages} from 'chrome://resources/ash/common/bluetooth/bluetooth_utils.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {BluetoothDeviceProperties, DeviceConnectionState} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './os_bluetooth_true_wireless_images.html.js';

const SettingsBluetoothTrueWirelessImagesElementBase =
    I18nMixin(PolymerElement);

export class SettingsBluetoothTrueWirelessImagesElement extends
    SettingsBluetoothTrueWirelessImagesElementBase {
  static get is() {
    return 'os-settings-bluetooth-true-wireless-images' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      device: {
        type: Object,
      },

      /**
       * Enum used as an ID for specific UI elements.
       * A BatteryType is passed between html and JS for
       * certain UI elements to determine their state.
       */
      BatteryType: {
        type: Object,
        value: BatteryType,
      },
    };
  }

  /* eslint-disable-next-line @typescript-eslint/naming-convention */
  BatteryType: BatteryType;
  device: BluetoothDeviceProperties;

  /**
   * Only show specific battery information if the device is
   * Connected and there exists information for that device.
   */
  private shouldShowBatteryTypeInfo_(
      device: BluetoothDeviceProperties, batteryType: BatteryType): boolean {
    if (device.connectionState !== DeviceConnectionState.kConnected ||
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
   */
  private shouldShowDefaultInfo_(device: BluetoothDeviceProperties): boolean {
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

  private isDeviceNotConnected_(device: BluetoothDeviceProperties): boolean {
    return device.connectionState === DeviceConnectionState.kNotConnected ||
        device.connectionState === DeviceConnectionState.kConnecting;
  }

  private getBatteryTypeString_(
      _device: BluetoothDeviceProperties, batteryType: BatteryType): string {
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

  private getImageSrc_(
      device: BluetoothDeviceProperties, batteryType: BatteryType): string {
    switch (batteryType) {
      case BatteryType.DEFAULT:
        return device.imageInfo!.defaultImageUrl!.url;
      case BatteryType.LEFT_BUD:
        return device.imageInfo!.trueWirelessImages!.leftBudImageUrl.url;
      case BatteryType.CASE:
        return device.imageInfo!.trueWirelessImages!.caseImageUrl.url;
      case BatteryType.RIGHT_BUD:
        return device.imageInfo!.trueWirelessImages!.rightBudImageUrl.url;
      default:
        assertNotReached();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsBluetoothTrueWirelessImagesElement.is]:
        SettingsBluetoothTrueWirelessImagesElement;
  }
}

customElements.define(
    SettingsBluetoothTrueWirelessImagesElement.is,
    SettingsBluetoothTrueWirelessImagesElement);
