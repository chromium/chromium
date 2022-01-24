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
import 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_icon.js';
import 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_device_battery_info.js';

import {I18nBehavior, I18nBehaviorInterface} from '//resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getBatteryPercentage, getDeviceName} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_utils.js';
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
    return getBatteryPercentage(device.deviceProperties) !== undefined;
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

    if (!shouldShowBatteryInfo) {
      return this.i18n(
          stringName, this.itemIndex + 1, this.listSize, deviceName);
    }

    const batteryPercentage = getBatteryPercentage(device.deviceProperties);
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