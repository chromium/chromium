// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Settings subpage providing high level summary of the state of Bluetooth and
 * its connected devices.
 */

import '../../settings_shared_css.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.m.js';

import {I18nBehavior, I18nBehaviorInterface} from '//resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getBluetoothConfig} from 'chrome://resources/cr_components/chromeos/bluetooth/cros_bluetooth_config.js';

import {loadTimeData} from '../../i18n_setup.js';
import {Router} from '../../router.js';
import {routes} from '../os_route.m.js';

const mojom = chromeos.bluetoothConfig.mojom;

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const SettingsBluetoothSummaryElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class SettingsBluetoothSummaryElement extends
    SettingsBluetoothSummaryElementBase {
  static get is() {
    return 'os-settings-bluetooth-summary';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @type {?chromeos.bluetoothConfig.mojom.BluetoothSystemProperties}
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
   * @param {!Event} e
   * @private
   */
  onSubpageArrowClick_(e) {
    this.navigateToBluetoothDevicesSubpage_();
    e.stopPropagation();
  }

  /** @private */
  navigateToBluetoothDevicesSubpage_() {
    Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICES);
  }

  /** @private */
  onWrapperClick_(e) {
    e.stopPropagation();
    // TODO(crbug.com/1010321): Check the state of Bluetooth properties
    // before opening subpage. Only open if enable Bluetooth toggle is enabled
    // and Bluetooth |systemState| is enabled.
    this.navigateToBluetoothDevicesSubpage_();
  }
}

customElements.define(
    SettingsBluetoothSummaryElement.is, SettingsBluetoothSummaryElement);