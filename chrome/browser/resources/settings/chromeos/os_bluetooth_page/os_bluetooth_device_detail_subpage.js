// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Settings subpage for managing Bluetooth device detail. This Element should
 * only be called when a device exist.
 */

import '../../settings_shared_css.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getDeviceName} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_utils.js';

import {Route, RouteObserverBehavior, RouteObserverBehaviorInterface, Router} from '../../router.js';
import {routes} from '../os_route.m.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {RouteObserverBehaviorInterface}
 */
const SettingsBluetoothDeviceDetailSubpageElementBase =
    mixinBehaviors([RouteObserverBehavior], PolymerElement);

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
