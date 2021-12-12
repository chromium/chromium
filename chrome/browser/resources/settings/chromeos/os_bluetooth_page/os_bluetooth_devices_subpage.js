// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Settings subpage for managing Bluetooth properties and devices.
 */

import '../../settings_shared_css.js';
import './os_paired_bluetooth_list.js';
import './settings_fast_pair_toggle.js';

import {BluetoothUiSurface, recordBluetoothUiSurfaceMetrics} from '//resources/cr_components/chromeos/bluetooth/bluetooth_metrics_utils.js';
import {I18nBehavior, I18nBehaviorInterface} from '//resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getBluetoothConfig} from 'chrome://resources/cr_components/chromeos/bluetooth/cros_bluetooth_config.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {Route} from '../../router.js';
import {routes} from '../os_route.m.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

const mojom = chromeos.bluetoothConfig.mojom;

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 */
const SettingsBluetoothDevicesSubpageElementBase =
    mixinBehaviors([I18nBehavior, RouteObserverBehavior], PolymerElement);

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
      /** Preferences state. */
      prefs: {
        type: Object,
        notify: true,
      },

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

      /**
       * Whether or not the fast pair feature flag is enabled which controls if
       * the fast pair toggle shows up.
       * @private {boolean}
       */
      isFastPairAllowed_: {
        type: Boolean,
        value: function() {
          return loadTimeData.getBoolean('enableFastPairFlag');
        }
      },

      /**
       * @private {!Array<!chromeos.bluetoothConfig.mojom.PairedBluetoothDeviceProperties>}
       */
      connectedDevices_: {
        type: Array,
        value: [],
      },

      /**
       * @private {!Array<!chromeos.bluetoothConfig.mojom.PairedBluetoothDeviceProperties>}
       */
      unconnectedDevices_: {
        type: Array,
        value: [],
      }
    };
  }

  /**
   * RouteObserverBehaviorInterface override
   * @param {!Route} route
   */
  currentRouteChanged(route) {
    if (route !== routes.BLUETOOTH_DEVICES) {
      return;
    }
    recordBluetoothUiSurfaceMetrics(
        BluetoothUiSurface.SETTINGS_DEVICE_LIST_SUBPAGE);
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

    this.connectedDevices_ = this.systemProperties.pairedDevices.filter(
        device => device.deviceProperties.connectionState !==
            mojom.DeviceConnectionState.kNotConnected);
    this.unconnectedDevices_ = this.systemProperties.pairedDevices.filter(
        device => device.deviceProperties.connectionState ===
            mojom.DeviceConnectionState.kNotConnected);
  }

  /**
   * Observer for isBluetoothToggleOn_ that returns early until the previous
   * value was not undefined to avoid wrongly toggling the Bluetooth state.
   * @param {boolean} newValue
   * @param {boolean} oldValue
   * @private
   */
  onBluetoothToggleChanged_(newValue, oldValue) {
    if (oldValue === undefined) {
      return;
    }
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

  /**
   * @param {!Array<!chromeos.bluetoothConfig.mojom.PairedBluetoothDeviceProperties>}
   *     devices
   * @return boolean
   * @private
   */
  shouldShowDeviceList_(devices) {
    return devices.length > 0;
  }

  /**
   * @return {boolean}
   * @private
   */
  shouldShowNoDevicesFound_() {
    return !this.connectedDevices_.length && !this.unconnectedDevices_.length;
  }
}

customElements.define(
    SettingsBluetoothDevicesSubpageElement.is,
    SettingsBluetoothDevicesSubpageElement);
