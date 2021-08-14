// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Settings subpage for managing Bluetooth device detail. This Element should
 * only be called when a device exist.
 */

import '../../settings_shared_css.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.m.js';

import {I18nBehavior, I18nBehaviorInterface} from '//resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getDeviceName} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_utils.js';

import {loadTimeData} from '../../i18n_setup.js';
import {Route, RouteObserverBehavior, RouteObserverBehaviorInterface, Router} from '../../router.js';
import {routes} from '../os_route.m.js';

const mojom = chromeos.bluetoothConfig.mojom;

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {RouteObserverBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 */
const SettingsBluetoothDeviceDetailSubpageElementBase =
    mixinBehaviors([RouteObserverBehavior, I18nBehavior], PolymerElement);

/** @polymer */
class SettingsBluetoothDeviceDetailSubpageElement extends
    SettingsBluetoothDeviceDetailSubpageElementBase {
  static get is() {
    return 'os-settings-bluetooth-device-detail-subpage';
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
      },

      /**
       * @private {?chromeos.bluetoothConfig.mojom.PairedBluetoothDeviceProperties}
       */
      device_: {
        type: Object,
        observer: 'onDeviceChanged_',
      },

      /**
       * Id of the currently paired device. This is set from the route query
       * parameters.
       * @private
       */
      deviceId_: {
        type: String,
        value: '',
      },

      /** @private */
      isDeviceConnected_: {
        reflectToAttribute: true,
        type: Boolean,
        computed: 'computeIsDeviceConnected_(device_.*)',
      },
    };
  }

  static get observers() {
    return [
      'onSystemPropertiesOrDeviceIdChanged_(systemProperties.*, deviceId_)',
    ];
  }

  /**
   * RouteObserverBehaviorInterface override
   * @param {!Route} route
   */
  currentRouteChanged(route) {
    if (route !== routes.BLUETOOTH_DEVICE_DETAIL) {
      return;
    }

    const queryParams = Router.getInstance().getQueryParameters();
    const deviceId = queryParams.get('id') || '';
    if (!deviceId) {
      console.error('No id specified for page:' + route);
      return;
    }
    this.deviceId_ = decodeURIComponent(deviceId);
  }

  /** @private */
  onSystemPropertiesOrDeviceIdChanged_() {
    if (!this.systemProperties || !this.deviceId_) {
      return;
    }

    const device = this.systemProperties.pairedDevices.find(
        (device) => device.deviceProperties.id === this.deviceId_);

    // Special case where the device was turned off or becomes unavailable
    // while user is vewing the page, return back to previous page.
    if (!device) {
      this.deviceId_ = '';
      Router.getInstance().navigateToPreviousRoute();
      return;
    }

    this.device_ = device;
  }

  /**
   * @return {string}
   * @private
   */
  getBluetoothStateIcon_() {
    return this.isDeviceConnected_ ? 'os-settings:bluetooth-connected' :
                                     'os-settings:bluetooth-disabled';
  }

  /**
   * @return {boolean}
   * @private
   */
  computeIsDeviceConnected_() {
    return this.device_.deviceProperties.connectionState ===
        mojom.DeviceConnectionState.kConnected;
  }

  /**
   * @return {string}
   * @private
   */
  getBluetoothStateBtnLabel_() {
    return this.isDeviceConnected_ ? this.i18n('bluetoothDisconnect') :
                                     this.i18n('bluetoothConnect');
  }

  /**
   * @return {string}
   * @private
   */
  getBluetoothStateTextLabel_() {
    return this.isDeviceConnected_ ?
        this.i18n('bluetoothDeviceDetailConnected') :
        this.i18n('bluetoothDeviceDetailDisconnected');
  }

  /**
   * @return {string}
   * @private
   */
  getDeviceName_() {
    return getDeviceName(this.device_);
  }

  /**
   * @return {boolean}
   * @private
   */
  shouldShowStateBtn_() {
    return this.device_.deviceProperties.audioCapability ===
        mojom.AudioOutputCapability.kCapableOfAudioOutput;
  }

  /** @private */
  onDeviceChanged_() {
    if (!this.device_) {
      return;
    }
    this.parentNode.pageTitle = getDeviceName(this.device_);
  }
}

customElements.define(
    SettingsBluetoothDeviceDetailSubpageElement.is,
    SettingsBluetoothDeviceDetailSubpageElement);
