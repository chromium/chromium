// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Settings subpage for managing Bluetooth properties and devices.
 */

import '../../settings_shared_css.js';
import './os_paired_bluetooth_list.js';

import {I18nBehavior, I18nBehaviorInterface} from '//resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getBluetoothConfig} from 'chrome://resources/cr_components/chromeos/bluetooth/cros_bluetooth_config.js';

const mojom = chromeos.bluetoothConfig.mojom;

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const SettingsBluetoothDevicesSubpageElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class SettingsBluetoothDevicesSubpageElement extends
    SettingsBluetoothDevicesSubpageElementBase {
  static get is() {
    return 'os-settings-bluetooth-devices-subpage';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @type {!chromeos.bluetoothConfig.mojom.BluetoothSystemProperties}
       */
      systemProperties: {
        type: Object,
        observer: 'onSystemPropertiesChanged_',
      },

      /**
       * Reflects the current state of the toggle button. This will be set when
       * the |systemProperties| state changes or when the user presses the
       * toggle.
       * @private
       */
      isBluetoothToggleOn_: {
        type: Boolean,
        observer: 'onBluetoothToggleChanged_',
      },
    };
  }

  /** @private */
  onSystemPropertiesChanged_() {
    if (this.isToggleDisabled_()) {
      return;
    }
    this.isBluetoothToggleOn_ = this.systemProperties.systemState ===
            mojom.BluetoothSystemState.kEnabled ||
        this.systemProperties.systemState ===
            mojom.BluetoothSystemState.kEnabling;
  }

  /** @private */
  onBluetoothToggleChanged_() {
    getBluetoothConfig().setBluetoothEnabledState(this.isBluetoothToggleOn_);
  }

  /**
   * @return {boolean}
   * @private
   */
  isToggleDisabled_() {
    // TODO(crbug.com/1010321): Add check for modification state when variable
    // is available.
    return this.systemProperties.systemState ===
        mojom.BluetoothSystemState.kUnavailable;
  }

  /**
   * @param {boolean} isBluetoothToggleOn
   * @param {string} onString
   * @param {string} offString
   * @return {string}
   * @private
   */
  getOnOffString_(isBluetoothToggleOn, onString, offString) {
    return isBluetoothToggleOn ? onString : offString;
  }
}

customElements.define(
    SettingsBluetoothDevicesSubpageElement.is,
    SettingsBluetoothDevicesSubpageElement);