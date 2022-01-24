// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Settings dialog is used to change a Bluetooth device nickname.
 */

import '../../settings_shared_css.js';
import '//resources/cr_elements/cr_input/cr_input.m.js';

import {I18nBehavior, I18nBehaviorInterface} from '//resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getDeviceName} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_utils.js';
import {getBluetoothConfig} from 'chrome://resources/cr_components/chromeos/bluetooth/cros_bluetooth_config.js';

import {loadTimeData} from '../../i18n_setup.js';

const mojom = chromeos.bluetoothConfig.mojom;

/** @type {number} */
const MAX_INPUT_LENGTH = 32;

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const SettingsBluetoothChangeDeviceNameDialogElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class SettingsBluetoothChangeDeviceNameDialogElement extends
    SettingsBluetoothChangeDeviceNameDialogElementBase {
  static get is() {
    return 'os-settings-bluetooth-change-device-name-dialog';
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

      /** Used to reference the MAX_INPUT_LENGTH constant in HTML. */
      MAX_INPUT_LENGTH: {
        type: Number,
        value: MAX_INPUT_LENGTH,
      },

      /** @private */
      deviceName_: {
        type: String,
        value: '',
        observer: 'onDeviceNameChanged_',
      },

      /** @private */
      isInputInvalid_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      }
    };
  }

  /** @private */
  onDeviceChanged_() {
    this.deviceName_ = getDeviceName(this.device);
  }

  /** @private */
  onCancelClick_(event) {
    this.$.dialog.close();
  }

  /** @private */
  onDoneClick_() {
    getBluetoothConfig().setDeviceNickname(
        this.device.deviceProperties.id, this.deviceName_);
    this.$.dialog.close();
  }

  /**
   * Returns a formatted string containing the current number of characters
   * entered in the input compared to the maximum number of characters allowed.
   * @param {string} deviceName
   * @return {string}
   * @private
   */
  getInputCountString_(deviceName) {
    // minimumIntegerDigits is 2 because we want to show a leading zero if
    // length is less than 10.
    return this.i18n(
        'bluetoothChangeNameDialogInputCharCount',
        deviceName.length.toLocaleString(
            /*locales=*/ undefined, {minimumIntegerDigits: 2}),
        MAX_INPUT_LENGTH.toLocaleString());
  }

  /**
   * Observer for deviceName_ that sanitizes its value by truncating it to
   * MAX_INPUT_LENGTH. This method will be recursively called until deviceName_
   * is fully sanitized.
   * @param {string} newValue
   * @param {string} oldValue
   * @private
   */
  onDeviceNameChanged_(newValue, oldValue) {
    if (oldValue) {
      // If oldValue.length > MAX_INPUT_LENGTH, the user attempted to
      // enter more than the max limit, this method was called and it was
      // truncated, and then this method was called one more time.
      this.isInputInvalid_ = oldValue.length > MAX_INPUT_LENGTH;
    } else {
      this.isInputInvalid_ = false;
    }

    // Truncate the name to MAX_INPUT_LENGTH.
    this.deviceName_ = this.deviceName_.substring(0, MAX_INPUT_LENGTH);
  }

  /**
   * @return {boolean}
   * @private
   */
  isDoneDisabled_() {
    if (this.deviceName_ === getDeviceName(this.device)) {
      return true;
    }

    if (!this.deviceName_.length) {
      return true;
    }

    return false;
  }
}

customElements.define(
    SettingsBluetoothChangeDeviceNameDialogElement.is,
    SettingsBluetoothChangeDeviceNameDialogElement);
