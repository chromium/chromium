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
import {assert} from 'chrome://resources/js/assert.m.js';
import {FocusRowBehavior, FocusRowBehaviorInterface} from 'chrome://resources/js/cr/ui/focus_row_behavior.m.js';

import {Router} from '../../router.js';
import {routes} from '../os_route.m.js';

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
      label = label +
          this.i18n(
              'bluetoothPairedDeviceItemA11yLabelLeftBudBattery',
              leftBudBatteryPercentage);
    }

    const caseBatteryPercentage =
        getBatteryPercentage(device.deviceProperties, BatteryType.CASE);
    if (caseBatteryPercentage !== undefined) {
      label = label +
          this.i18n(
              'bluetoothPairedDeviceItemA11yLabelCaseBattery',
              caseBatteryPercentage);
    }

    const rightBudbatteryPercentage =
        getBatteryPercentage(device.deviceProperties, BatteryType.RIGHT_BUD);
    if (rightBudbatteryPercentage !== undefined) {
      label = label +
          this.i18n(
              'bluetoothPairedDeviceItemA11yLabelRightBudBattery',
              rightBudbatteryPercentage);
    }

    return label;
  }

  /**
   * @param {!chromeos.bluetoothConfig.mojom.PairedBluetoothDeviceProperties}
   *     device
   * @return {string}
   * @private
   */
  getMultipleBatteryAriaLabel_(device) {
    const deviceName = this.getDeviceName_(device);
    const deviceType = chromeos.bluetoothConfig.mojom.DeviceType;
    let stringName;
    switch (device.deviceProperties.deviceType) {
      case deviceType.kComputer:
        stringName = 'bluetoothPairedDeviceItemA11yLabelTypeComputer';
        break;
      case deviceType.kPhone:
        stringName = 'bluetoothPairedDeviceItemA11yLabelTypePhone';
        break;
      case deviceType.kHeadset:
        stringName = 'bluetoothPairedDeviceItemA11yLabelTypeHeadset';
        break;
      case deviceType.kVideoCamera:
        stringName = 'bluetoothPairedDeviceItemA11yLabelTypeVideoCamera';
        break;
      case deviceType.kGameController:
        stringName = 'bluetoothPairedDeviceItemA11yLabelTypeGameController';
        break;
      case deviceType.kKeyboard:
        stringName = 'bluetoothPairedDeviceItemA11yLabelTypeKeyboard';
        break;
      case deviceType.kMouse:
        stringName = 'bluetoothPairedDeviceItemA11yLabelTypeMouse';
        break;
      case deviceType.kTablet:
        stringName = 'bluetoothPairedDeviceItemA11yLabelTypeTablet';
        break;
      default:
        stringName = 'bluetoothPairedDeviceItemA11yLabelTypeUnknown';
    }

    return this.i18n(
               stringName, this.itemIndex + 1, this.listSize, deviceName) +
        this.getMultipleBatteryPercentageString_(device);
  }

  /**
   * @param {!chromeos.bluetoothConfig.mojom.PairedBluetoothDeviceProperties}
   *     device
   * @return {string}
   * @private
   */
  getAriaLabel_(device) {
    // If there are multiple batteries, then we will concatenate the label
    // describing the battery percentage of each available true wireless
    // component with only the label describing the device, thus we can
    // skip the logic below that is used for labels describing default
    // battery information, or none, if no battery information is available.
    if (hasAnyDetailedBatteryInfo(device.deviceProperties)) {
      return this.getMultipleBatteryAriaLabel_(device);
    }

    const deviceName = this.getDeviceName_(device);
    const deviceType = chromeos.bluetoothConfig.mojom.DeviceType;
    const shouldShowBatteryInfo = this.shouldShowBatteryInfo_(device);
    let stringName;
    switch (device.deviceProperties.deviceType) {
      case deviceType.kComputer:
        stringName = shouldShowBatteryInfo ?
            'bluetoothPairedDeviceItemA11yLabelTypeComputerWithBatteryInfo' :
            'bluetoothPairedDeviceItemA11yLabelTypeComputer';
        break;
      case deviceType.kPhone:
        stringName = shouldShowBatteryInfo ?
            'bluetoothPairedDeviceItemA11yLabelTypePhoneWithBatteryInfo' :
            'bluetoothPairedDeviceItemA11yLabelTypePhone';
        break;
      case deviceType.kHeadset:
        stringName = shouldShowBatteryInfo ?
            'bluetoothPairedDeviceItemA11yLabelTypeHeadsetWithBatteryInfo' :
            'bluetoothPairedDeviceItemA11yLabelTypeHeadset';
        break;
      case deviceType.kVideoCamera:
        stringName = shouldShowBatteryInfo ?
            'bluetoothPairedDeviceItemA11yLabelTypeVideoCameraWithBatteryInfo' :
            'bluetoothPairedDeviceItemA11yLabelTypeVideoCamera';
        break;
      case deviceType.kGameController:
        stringName = shouldShowBatteryInfo ?
            'bluetoothPairedDeviceItemA11yLabelTypeGameControllerWithBatteryInfo' :
            'bluetoothPairedDeviceItemA11yLabelTypeGameController';
        break;
      case deviceType.kKeyboard:
        stringName = shouldShowBatteryInfo ?
            'bluetoothPairedDeviceItemA11yLabelTypeKeyboardWithBatteryInfo' :
            'bluetoothPairedDeviceItemA11yLabelTypeKeyboard';
        break;
      case deviceType.kMouse:
        stringName = shouldShowBatteryInfo ?
            'bluetoothPairedDeviceItemA11yLabelTypeMouseWithBatteryInfo' :
            'bluetoothPairedDeviceItemA11yLabelTypeMouse';
        break;
      case deviceType.kTablet:
        stringName = shouldShowBatteryInfo ?
            'bluetoothPairedDeviceItemA11yLabelTypeTabletWithBatteryInfo' :
            'bluetoothPairedDeviceItemA11yLabelTypeTablet';
        break;
      default:
        stringName = shouldShowBatteryInfo ?
            'bluetoothPairedDeviceItemA11yLabelTypeUnknownWithBatteryInfo' :
            'bluetoothPairedDeviceItemA11yLabelTypeUnknown';
    }

    // If we get to this point and we should show battery information, then
    // the battery information is only describing the default battery.
    if (shouldShowBatteryInfo) {
      const batteryPercentage =
          getBatteryPercentage(device.deviceProperties, BatteryType.DEFAULT);
      assert(batteryPercentage !== undefined);
      return this.i18n(
          stringName, this.itemIndex + 1, this.listSize, deviceName,
          batteryPercentage);
    }

    // The default contains no battery information in the label.
    return this.i18n(stringName, this.itemIndex + 1, this.listSize, deviceName);
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
