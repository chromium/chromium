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

import {loadTimeData} from '../../i18n_setup.js';

const mojom = chromeos.bluetoothConfig.mojom;

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

      /** @private */
      deviceName_: {
        type: String,
        value: '',
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
    this.$.dialog.close();
  }
}

customElements.define(
    SettingsBluetoothChangeDeviceNameDialogElement.is,
    SettingsBluetoothChangeDeviceNameDialogElement);
