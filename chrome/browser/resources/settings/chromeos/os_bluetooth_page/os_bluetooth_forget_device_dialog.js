// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Forget dialog is used to forget a Bluetooth device.
 */
import '../../settings_shared.css.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import {getDeviceName} from 'chrome://resources/ash/common/bluetooth/bluetooth_utils.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {PairedBluetoothDeviceProperties} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const SettingsBluetoothForgetDeviceDialogElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);
/** @polymer */
class SettingsBluetoothForgetDeviceDialogElement extends
    SettingsBluetoothForgetDeviceDialogElementBase {
  static get is() {
    return 'os-settings-bluetooth-forget-device-dialog';
  }
  static get template() {
    return html`{__html_template__}`;
  }
  static get properties() {
    return {
      /**
       * @private {!PairedBluetoothDeviceProperties}
       */
      device_: {
        type: Object,
      },
    };
  }
  /**
   * @private
   */
  getForgetDeviceDialogBodyText_() {
    return this.i18n(
        'bluetoothDevicesDialogLabel', this.getDeviceName_(),
        loadTimeData.getString('primaryUserEmail'));
  }

  /**
   * @return {string}
   * @private
   */
  getDeviceName_() {
    return getDeviceName(this.device_);
  }

  /**
   * @param {!Event} event
   * @private
   */
  onForgetTap_(event) {
    const fireEvent = new CustomEvent(
        'forget-bluetooth-device', {bubbles: true, composed: true});
    this.dispatchEvent(fireEvent);
    this.$.dialog.close();
    event.stopPropagation();
  }

  /** @private */
  onCancelClick_(event) {
    this.$.dialog.close();
  }
}
customElements.define(
    SettingsBluetoothForgetDeviceDialogElement.is,
    SettingsBluetoothForgetDeviceDialogElement);
