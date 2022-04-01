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
import '//resources/cr_elements/icons.m.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_icon.js';
import 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_device_battery_info.js';

import {I18nBehavior, I18nBehaviorInterface} from '//resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {BatteryType} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_types.js';
import {getBatteryPercentage, getDeviceName, hasAnyDetailedBatteryInfo} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_utils.js';
import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {FocusRowBehavior, FocusRowBehaviorInterface} from 'chrome://resources/js/cr/ui/focus_row_behavior.m.js';

import {Router} from '../../router.js';
import {routes} from '../os_route.js';

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

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    // Fire an event in case the tooltip was previously showing for the managed
    // icon in this item and this item is being removed.
    this.fireTooltipStateChangeEvent_(/*showTooltip=*/ false);
  }

  /** @private */
  onDeviceChanged_() {
    if (!this.device) {
      return;
    }

    if (!this.device.deviceProperties.isBlockedByPolicy) {
      // Fire an event in case the tooltip was previously showing for this
      // icon and this icon now is hidden.
      this.fireTooltipStateChangeEvent_(/*showTooltip=*/ false);
    }
  }

  /**
   * @param {!KeyboardEvent} event
   * @private
   */
  onKeydown_(event) {
    if (event.key !== 'Enter' && event.key !== ' ') {
      return;
    }

    this.navigateToDetailPage_();
    event.stopPropagation();
  }

  /**
   * @param {!Event} event
   * @private
   */
  onSelected_(event) {
    this.navigateToDetailPage_();
    event.stopPropagation();
  }

  /** @private */
  navigateToDetailPage_() {
    const params = new URLSearchParams();
    params.append('id', this.device.deviceProperties.id);
    Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICE_DETAIL, params);
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
   * @return {boolean}
   * @private
   */
  shouldShowBatteryInfo_(device) {
    return getBatteryPercentage(
               device.deviceProperties, BatteryType.DEFAULT) !== undefined ||
        hasAnyDetailedBatteryInfo(device.deviceProperties);
  }

  /**
   * @param {!chromeos.bluetoothConfig.mojom.PairedBluetoothDeviceProperties}
   *     device
   * @return {string}
   * @private
   */
  getMultipleBatteryPercentageString_(device) {
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

  /**
   * @param {!chromeos.bluetoothConfig.mojom.PairedBluetoothDeviceProperties}
   *     device
   * @return {boolean}
   * @private
   */
  isDeviceConnecting_(device) {
    return device.deviceProperties.connectionState ===
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnecting;
  }

  /**
   * @param {!chromeos.bluetoothConfig.mojom.PairedBluetoothDeviceProperties}
   *     device
   * @return {string}
   * @private
   */
  getAriaLabel_(device) {
    // Start with the base information of the device name and location within
    // the list of devices with the same connection state.
    let a11yLabel = this.i18n(
        'bluetoothA11yDeviceName', this.itemIndex + 1, this.listSize,
        this.getDeviceName_(device));

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

  /**
   * @param {!chromeos.bluetoothConfig.mojom.PairedBluetoothDeviceProperties}
   *     device
   * @return {string}
   * @private
   */
  getA11yDeviceConnectionStatusTextName_(device) {
    const connectionState =
        chromeos.bluetoothConfig.mojom.DeviceConnectionState;
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

  /**
   * @param {!chromeos.bluetoothConfig.mojom.PairedBluetoothDeviceProperties}
   *     device
   * @return {string}
   * @private
   */
  getA11yDeviceTypeTextName_(device) {
    const deviceType = chromeos.bluetoothConfig.mojom.DeviceType;
    switch (device.deviceProperties.deviceType) {
      case deviceType.kUnknown:
        return 'bluetoothA11yDeviceTypeUnknown';
      case deviceType.kComputer:
        return 'bluetoothA11yDeviceTypeComputer';
      case deviceType.kPhone:
        return 'bluetoothA11yDeviceTypePhone';
      case deviceType.kHeadset:
        return 'bluetoothA11yDeviceTypeHeadset';
      case deviceType.kVideoCamera:
        return 'bluetoothA11yDeviceTypeVideoCamera';
      case deviceType.kGameController:
        return 'bluetoothA11yDeviceTypeGameController';
      case deviceType.kKeyboard:
        return 'bluetoothA11yDeviceTypeKeyboard';
      case deviceType.kKeyboardMouseCombo:
        return 'bluetoothA11yDeviceTypeKeyboardMouseCombo';
      case deviceType.kMouse:
        return 'bluetoothA11yDeviceTypeMouse';
      case deviceType.kTablet:
        return 'bluetoothA11yDeviceTypeTablet';
      default:
        assertNotReached();
    }
  }

  /** @private */
  onShowTooltip_() {
    this.fireTooltipStateChangeEvent_(/*showTooltip=*/ true);
  }

  /**
   * @param {boolean} showTooltip
   */
  fireTooltipStateChangeEvent_(showTooltip) {
    this.dispatchEvent(new CustomEvent('managed-tooltip-state-change', {
      bubbles: true,
      composed: true,
      detail: {
        address: this.device.deviceProperties.address,
        show: showTooltip,
        element: showTooltip ? this.shadowRoot.getElementById('managedIcon') :
                               undefined,
      }
    }));
  }
}

customElements.define(
    SettingsPairedBluetoothListItemElement.is,
    SettingsPairedBluetoothListItemElement);
