// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Item in <os-paired_bluetooth-list> that displays information for a paired
 * Bluetooth device.
 */

import '../settings_shared.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/ash/common/bluetooth/bluetooth_icon.js';
import 'chrome://resources/ash/common/bluetooth/bluetooth_device_battery_info.js';

import {BatteryType} from 'chrome://resources/ash/common/bluetooth/bluetooth_types.js';
import {getBatteryPercentage, getDeviceNameUnsafe, hasAnyDetailedBatteryInfo} from 'chrome://resources/ash/common/bluetooth/bluetooth_utils.js';
import {FocusRowMixin} from 'chrome://resources/ash/common/cr_elements/focus_row_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {DeviceConnectionState, DeviceType, PairedBluetoothDeviceProperties} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Router, routes} from '../router.js';

import {getTemplate} from './os_paired_bluetooth_list_item.html.js';

const SettingsPairedBluetoothListItemElementBase =
    FocusRowMixin(I18nMixin(PolymerElement));

export class SettingsPairedBluetoothListItemElement extends
    SettingsPairedBluetoothListItemElementBase {
  static get is() {
    return 'os-settings-paired-bluetooth-list-item' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      device: {
        type: Object,
        observer: 'onDeviceChanged_',
      },

      /** The index of this item in its parent list, used for its a11y label. */
      itemIndex: Number,

      /**
       * The total number of elements in this item's parent list, used for its
       * a11y label.
       */
      listSize: Number,
    };
  }

  device: PairedBluetoothDeviceProperties;
  itemIndex: number;
  listSize: number;

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    // Fire an event in case the tooltip was previously showing for the managed
    // icon in this item and this item is being removed.
    this.fireTooltipStateChangeEvent_(/*showTooltip=*/ false);
  }

  private onDeviceChanged_(): void {
    if (!this.device) {
      return;
    }

    if (!this.device.deviceProperties.isBlockedByPolicy) {
      // Fire an event in case the tooltip was previously showing for this
      // icon and this icon now is hidden.
      this.fireTooltipStateChangeEvent_(/*showTooltip=*/ false);
    }
  }

  private onKeydown_(event: KeyboardEvent): void {
    if (event.key !== 'Enter' && event.key !== ' ') {
      return;
    }

    this.navigateToDetailPage_();
    event.stopPropagation();
  }

  private onSelected_(event: Event): void {
    this.navigateToDetailPage_();
    event.stopPropagation();
  }

  private navigateToDetailPage_(): void {
    const params = new URLSearchParams();
    params.append('id', this.device.deviceProperties.id);
    Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICE_DETAIL, params);
  }

  private getDeviceNameUnsafe_(device: PairedBluetoothDeviceProperties):
      string {
    return getDeviceNameUnsafe(device);
  }

  private shouldShowBatteryInfo_(device: PairedBluetoothDeviceProperties):
      boolean {
    return getBatteryPercentage(
               device.deviceProperties, BatteryType.DEFAULT) !== undefined ||
        hasAnyDetailedBatteryInfo(device.deviceProperties);
  }

  private getMultipleBatteryPercentageString_(
      device: PairedBluetoothDeviceProperties): string {
    let label = '';
    const leftBudBatteryPercentage =
        getBatteryPercentage(device.deviceProperties, BatteryType.LEFT_BUD);
    if (leftBudBatteryPercentage !== undefined) {
      label += ' ' +
          this.i18n(
              'bluetoothA11yDeviceNamedBatteryInfoLeftBud',
              leftBudBatteryPercentage);
    }

    const caseBatteryPercentage =
        getBatteryPercentage(device.deviceProperties, BatteryType.CASE);
    if (caseBatteryPercentage !== undefined) {
      label += ' ' +
          this.i18n(
              'bluetoothA11yDeviceNamedBatteryInfoCase', caseBatteryPercentage);
    }

    const rightBudbatteryPercentage =
        getBatteryPercentage(device.deviceProperties, BatteryType.RIGHT_BUD);
    if (rightBudbatteryPercentage !== undefined) {
      label += ' ' +
          this.i18n(
              'bluetoothA11yDeviceNamedBatteryInfoRightBud',
              rightBudbatteryPercentage);
    }

    return label;
  }

  private isDeviceConnecting_(device: PairedBluetoothDeviceProperties):
      boolean {
    return device.deviceProperties.connectionState ===
        DeviceConnectionState.kConnecting;
  }

  /**
   * @param {!PairedBluetoothDeviceProperties}
   *     device
   * @return {string}
   * @private
   */
  private getAriaLabel_(device: PairedBluetoothDeviceProperties): string {
    // Start with the base information of the device name and location within
    // the list of devices with the same connection state.
    let a11yLabel = loadTimeData.getStringF(
        'bluetoothA11yDeviceName', this.itemIndex + 1, this.listSize,
        this.getDeviceNameUnsafe_(device));

    // Include the connection status.
    a11yLabel +=
        ' ' + this.i18n(this.getA11yDeviceConnectionStatusTextName_(device));

    // Include the device type.
    a11yLabel += ' ' + this.i18n(this.getA11yDeviceTypeTextName_(device));

    // Include any available battery information.
    if (hasAnyDetailedBatteryInfo(device.deviceProperties)) {
      a11yLabel += this.getMultipleBatteryPercentageString_(device);
    } else if (this.shouldShowBatteryInfo_(device)) {
      const batteryPercentage =
          getBatteryPercentage(device.deviceProperties, BatteryType.DEFAULT);
      assert(batteryPercentage !== undefined);
      a11yLabel +=
          ' ' + this.i18n('bluetoothA11yDeviceBatteryInfo', batteryPercentage);
    }
    return a11yLabel;
  }

  private getA11yDeviceConnectionStatusTextName_(
      device: PairedBluetoothDeviceProperties): string {
    const connectionState = DeviceConnectionState;
    switch (device.deviceProperties.connectionState) {
      case connectionState.kConnected:
        return 'bluetoothA11yDeviceConnectionStateConnected';
      case connectionState.kConnecting:
        return 'bluetoothA11yDeviceConnectionStateConnecting';
      case connectionState.kNotConnected:
        return 'bluetoothA11yDeviceConnectionStateNotConnected';
      default:
        assertNotReached();
    }
  }

  private getA11yDeviceTypeTextName_(device: PairedBluetoothDeviceProperties):
      string {
    switch (device.deviceProperties.deviceType) {
      case DeviceType.kUnknown:
        return 'bluetoothA11yDeviceTypeUnknown';
      case DeviceType.kComputer:
        return 'bluetoothA11yDeviceTypeComputer';
      case DeviceType.kPhone:
        return 'bluetoothA11yDeviceTypePhone';
      case DeviceType.kHeadset:
        return 'bluetoothA11yDeviceTypeHeadset';
      case DeviceType.kVideoCamera:
        return 'bluetoothA11yDeviceTypeVideoCamera';
      case DeviceType.kGameController:
        return 'bluetoothA11yDeviceTypeGameController';
      case DeviceType.kKeyboard:
        return 'bluetoothA11yDeviceTypeKeyboard';
      case DeviceType.kKeyboardMouseCombo:
        return 'bluetoothA11yDeviceTypeKeyboardMouseCombo';
      case DeviceType.kMouse:
        return 'bluetoothA11yDeviceTypeMouse';
      case DeviceType.kTablet:
        return 'bluetoothA11yDeviceTypeTablet';
      default:
        assertNotReached();
    }
  }

  private onShowTooltip_(): void {
    this.fireTooltipStateChangeEvent_(/*showTooltip=*/ true);
  }

  private fireTooltipStateChangeEvent_(showTooltip: boolean): void {
    this.dispatchEvent(new CustomEvent('managed-tooltip-state-change', {
      bubbles: true,
      composed: true,
      detail: {
        address: this.device.deviceProperties.address,
        show: showTooltip,
        element: showTooltip ? this.shadowRoot!.getElementById('managedIcon') :
                               undefined,
      },
    }));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsPairedBluetoothListItemElement.is]:
        SettingsPairedBluetoothListItemElement;
  }
}

customElements.define(
    SettingsPairedBluetoothListItemElement.is,
    SettingsPairedBluetoothListItemElement);
