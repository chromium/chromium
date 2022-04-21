// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Settings page for managing Bluetooth properties and devices. This page
 * provides a high-level summary and routing to subpages
 */

import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import '../../settings_shared_css.js';
import '../../settings_page/settings_animated_pages.js';
import './os_bluetooth_devices_subpage.js';
import './os_bluetooth_summary.js';
import './os_bluetooth_device_detail_subpage.js';
import './os_bluetooth_pairing_dialog.js';

import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getBluetoothConfig} from 'chrome://resources/cr_components/chromeos/bluetooth/cros_bluetooth_config.js';
import {BluetoothSystemProperties, BluetoothSystemState, SystemPropertiesObserverInterface, SystemPropertiesObserverReceiver} from 'chrome://resources/mojo/chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';

/**
 * @constructor
 * @extends {PolymerElement}
 */
const SettingsBluetoothPageElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class SettingsBluetoothPageElement extends SettingsBluetoothPageElementBase {
  static get is() {
    return 'os-settings-bluetooth-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** Preferences state. */
      prefs: {
        type: Object,
        notify: true,
      },

      /**
       * @private {!BluetoothSystemProperties}
       */
      systemProperties_: Object,

      /** @private */
      shouldShowPairingDialog_: {
        type: Boolean,
        value: false,
      },
    };
  }

  constructor() {
    super();

    /**
     * @private {!SystemPropertiesObserverReceiver}
     */
    this.systemPropertiesObserverReceiver_ =
        new SystemPropertiesObserverReceiver(
            /**
             * @type {!SystemPropertiesObserverInterface}
             */
            (this));
  }

  ready() {
    super.ready();
    getBluetoothConfig().observeSystemProperties(
        this.systemPropertiesObserverReceiver_.$.bindNewPipeAndPassRemote());
  }

  /**
   * SystemPropertiesObserverInterface override
   * @param {!BluetoothSystemProperties}
   *     properties
   */
  onPropertiesUpdated(properties) {
    this.systemProperties_ = properties;
  }

  /** @private */
  onStartPairing_() {
    this.shouldShowPairingDialog_ = true;
  }

  /** @private */
  onClosePairingDialog_() {
    this.shouldShowPairingDialog_ = false;
  }

  /**
   * @return {boolean}
   * @private
   */
  shouldShowPairNewDevice_() {
    if (!this.systemProperties_) {
      return false;
    }

    return this.systemProperties_.systemState ===
        BluetoothSystemState.kEnabled ||
        this.systemProperties_.systemState === BluetoothSystemState.kEnabling;
  }
}

customElements.define(
    SettingsBluetoothPageElement.is, SettingsBluetoothPageElement);
