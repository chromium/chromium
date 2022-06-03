// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './diagnostics_card_frame.js';
import './icons.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ConnectionType, KeyboardInfo, TouchDeviceInfo} from './diagnostics_types.js';

/**
 * @fileoverview
 * 'input-card' is responsible for displaying a list of input devices with links
 * to their testers.
 */

/**
 * Enum of device types supported by input-card elements.
 * @enum {string}
 */
export const InputCardType = {
  kKeyboard: 'keyboard',
  kTouchpad: 'touchpad',
  kTouchscreen: 'touchscreen',
};

Polymer({
  is: 'input-card',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /**
     * The type of input device to be displayed. Valid values are 'keyboard',
     * 'touchpad', and 'touchscreen'.
     * @type {!InputCardType}
     */
    deviceType: String,

    /** @type {!Array<!KeyboardInfo|!TouchDeviceInfo>} */
    devices: {
      type: Array,
      value: () => [],
    },

    deviceIcon_: {
      type: String,
      computed: 'computeDeviceIcon_(deviceType)',
    },
  },

  computeDeviceIcon_(deviceType) {
    return {
      [InputCardType.kKeyboard]: 'diagnostics:keyboard',
      [InputCardType.kTouchpad]: 'diagnostics:touchpad',
      [InputCardType.kTouchscreen]: 'diagnostics:touchscreen',
    }[deviceType];
  },

  /**
   * Fetches the description string for a device based on its connection type
   * (e.g. "Bluetooth keyboard", "Internal touchpad").
   * @param {!KeyboardInfo|!TouchDeviceInfo} device
   * @return {string}
   * @private
   */
  getDeviceDescription_(device) {
    if (device.connectionType === ConnectionType.kUnknown) {
      return '';
    }
    const connectionTypeString = {
      [ConnectionType.kInternal]: 'Internal',
      [ConnectionType.kUsb]: 'Usb',
      [ConnectionType.kBluetooth]: 'Bluetooth',
    }[device.connectionType];
    const deviceTypeString = {
      [InputCardType.kKeyboard]: 'Keyboard',
      [InputCardType.kTouchpad]: 'Touchpad',
      [InputCardType.kTouchscreen]: 'Touchscreen',
    }[this.deviceType];
    return loadTimeData.getString(
        'inputDescription' + connectionTypeString + deviceTypeString);
  },

  /**
   * @param {!PointerEvent} e
   * @private
   */
  handleTestButtonClick_(e) {
    const evdevId =
        parseInt(e.target.closest('.device').getAttribute('data-evdev-id'), 10);
    this.dispatchEvent(new CustomEvent(
        'test-button-click', {composed: true, detail: {evdevId: evdevId}}));
  },
});
