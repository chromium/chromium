// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * View displaying a dynamically colored/sized battery icon and
 * corresponding battery percentage string for a given device and battery
 * type.
 */

import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import './bluetooth_icons.html.js';

import {I18nMixin} from '//resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from '//resources/js/assert.js';
import {BluetoothDeviceProperties} from '//resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {IronIconElement} from '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './bluetooth_battery_icon_percentage.html.js';
import {BatteryType} from './bluetooth_types.js';
import {getBatteryPercentage} from './bluetooth_utils.js';

/**
 * The threshold percentage where any battery percentage lower is considered
 * 'low battery'.
 */
const LOW_BATTERY_THRESHOLD_PERCENTAGE: number = 25;

/**
 * Ranges for each battery icon, where the value of the first index is the
 * minimum battery percentage in the range (inclusive), and the second index is
 * the maximum battery percentage in the range (inclusive).
 */
const BATTERY_ICONS_RANGES: number[][] = [
  [0, 7],
  [8, 14],
  [15, 21],
  [22, 28],
  [29, 35],
  [36, 42],
  [43, 49],
  [50, 56],
  [57, 63],
  [64, 70],
  [71, 77],
  [78, 85],
  [86, 92],
  [93, 100],
];

const BluetoothBatteryIconPercentageElementBase = I18nMixin(PolymerElement);

export interface BluetoothBatteryIconPercentageElement {
  $: {
    batteryPercentage: HTMLElement,
    batteryIcon: IronIconElement,
  };
}

export class BluetoothBatteryIconPercentageElement extends
    BluetoothBatteryIconPercentageElementBase {
  static get is() {
    return 'bluetooth-battery-icon-percentage' as const;
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
       * The BatteryType of this component.
       */
      batteryType: {
        type: Object,
      },

      /**
       * Boolean used to reflect whether the percentage should be labeled
       * with the battery type, e.g. (Left).
       */
      isTypeLabeled: {type: Boolean, default: false},

      isLowBattery_: {
        reflectToAttribute: true,
        type: Boolean,
        computed: 'computeIsLowBattery_(device, batteryType)',
      },

      isMultipleBattery_: {
        reflectToAttribute: true,
        type: Boolean,
        computed: 'computeIsMultipleBattery_(batteryType)',
      },
    };
  }

  device: BluetoothDeviceProperties;
  batteryType: BatteryType;
  isTypeLabeled: boolean;
  private isLowBattery_: boolean;
  private isMultipleBattery_: boolean;

  private computeIsLowBattery_(device: BluetoothDeviceProperties,
      batteryType: BatteryType): boolean {
    const batteryPercentage = getBatteryPercentage(device, batteryType);
    if (batteryPercentage === undefined) {
      return false;
    }
    return batteryPercentage < LOW_BATTERY_THRESHOLD_PERCENTAGE;
  }


  private computeIsMultipleBattery_(batteryType: BatteryType): boolean {
    switch (batteryType) {
      case BatteryType.LEFT_BUD:
      case BatteryType.CASE:
      case BatteryType.RIGHT_BUD:
        return true;
      case BatteryType.DEFAULT:
      default:
        return false;
    }
  }

  private getBatteryPercentageString_(device: BluetoothDeviceProperties,
      batteryType: BatteryType): string {
    const batteryPercentage = getBatteryPercentage(device, batteryType);
    if (batteryPercentage === undefined) {
      return '';
    }

    // If unlabeled, don't add the battery type to the percentage string.
    if (!this.isTypeLabeled) {
      return this.i18n(
          'bluetoothPairedDeviceItemBatteryPercentage', batteryPercentage);
    }

    switch (batteryType) {
      case BatteryType.DEFAULT:
        return this.i18n(
            'bluetoothPairedDeviceItemBatteryPercentage', batteryPercentage);
      case BatteryType.LEFT_BUD:
        return this.i18n(
            'bluetoothPairedDeviceItemLeftBudTrueWirelessBatteryPercentage',
            batteryPercentage);
      case BatteryType.CASE:
        return this.i18n(
            'bluetoothPairedDeviceItemCaseTrueWirelessBatteryPercentage',
            batteryPercentage);
      case BatteryType.RIGHT_BUD:
        return this.i18n(
            'bluetoothPairedDeviceItemRightBudTrueWirelessBatteryPercentage',
            batteryPercentage);
    }
  }

  private getBatteryIcon_(device: BluetoothDeviceProperties,
      batteryType: BatteryType): string {
    const batteryPercentage = getBatteryPercentage(device, batteryType);
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

    return 'bluetooth:battery-' + range[0] + '-' + range[1];
  }

  getIsLowBatteryForTest(): boolean {
    return this.isLowBattery_;
  }
}

declare global {
    interface HTMLElementTagNameMap {
      [BluetoothBatteryIconPercentageElement.is]:
      BluetoothBatteryIconPercentageElement;
    }
}

customElements.define(
    BluetoothBatteryIconPercentageElement.is,
    BluetoothBatteryIconPercentageElement);
