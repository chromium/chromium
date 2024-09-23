// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * View displaying Bluetooth device battery information.
 */

import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import './bluetooth_battery_icon_percentage.js';

import {BluetoothDeviceProperties} from '//resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './bluetooth_device_battery_info.html.js';
import {BatteryType} from './bluetooth_types.js';
import {getBatteryPercentage, hasAnyDetailedBatteryInfo} from './bluetooth_utils.js';

export class BluetoothDeviceBatteryInfoElement extends PolymerElement {
  static get is() {
    return 'bluetooth-device-battery-info' as const;
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

      showMultipleBatteries_: {
        type: Boolean,
        computed: 'computeShowMultipleBatteries_(device)',
      },
    };
  }

  device: BluetoothDeviceProperties;
  private showMultipleBatteries_: boolean;

  private computeShowMultipleBatteries_(device: BluetoothDeviceProperties): boolean {
    return hasAnyDetailedBatteryInfo(device);
  }

  private shouldShowBattery_(device: BluetoothDeviceProperties,
      batteryType: BatteryType): boolean {
    return getBatteryPercentage(device, batteryType) !== undefined;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [BluetoothDeviceBatteryInfoElement.is]:
    BluetoothDeviceBatteryInfoElement;
  }
}

customElements.define(
    BluetoothDeviceBatteryInfoElement.is,
    BluetoothDeviceBatteryInfoElement);
