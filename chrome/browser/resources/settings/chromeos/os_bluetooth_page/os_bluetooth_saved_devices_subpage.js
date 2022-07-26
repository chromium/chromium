// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Settings subpage for managing Bluetooth saved devices.
 */

import '../../settings_shared.css.js';
import './os_saved_devices_list.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route, Router} from '../../router.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

import {OsBluetoothDevicesSubpageBrowserProxy, OsBluetoothDevicesSubpageBrowserProxyImpl} from './os_bluetooth_devices_subpage_browser_proxy.js';
import {FastPairSavedDevice} from './settings_fast_pair_constants.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 */
const SettingsBluetoothSavedDevicesSubpageElementBase = mixinBehaviors(
    [
      I18nBehavior,
      WebUIListenerBehavior,
      RouteObserverBehavior,
    ],
    PolymerElement);

/** @polymer */
class SettingsBluetoothSavedDevicesSubpageElement extends
    SettingsBluetoothSavedDevicesSubpageElementBase {
  static get is() {
    return 'os-settings-bluetooth-saved-devices-subpage';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @protected
       */
      savedDevicesSublabel_: {
        type: String,
        value() {
          return loadTimeData.getString('sublabelWithEmail');
        },
      },

      /**
       * @protected {!Array<!FastPairSavedDevice>}
       */
      savedDevices_: {
        type: Array,
        value: [],
      },
    };
  }

  constructor() {
    super();
    this.parentNode.pageTitle = loadTimeData.getString('savedDevicesPageName');
    /** @private {?OsBluetoothDevicesSubpageBrowserProxy} */
    this.browserProxy_ =
        OsBluetoothDevicesSubpageBrowserProxyImpl.getInstance();
  }

  /** @override */
  ready() {
    super.ready();
    this.addWebUIListener(
        'fast-pair-saved-devices-list', this.getSavedDevices_.bind(this));
  }

  /**
   * RouteObserverBehaviorInterface override
   * @param {!Route} route
   * @param {!Route=} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // If we're navigating to the Saved Devices page, fetch the devices.
    if (route === routes.BLUETOOTH_SAVED_DEVICES) {
      this.browserProxy_.requestFastPairSavedDevices();
      return;
    }
  }

  /**
   * @param {!Array<!FastPairSavedDevice>} devices
   * @private
   */
  getSavedDevices_(devices) {
    this.savedDevices_ = devices.slice(0);
  }
}

customElements.define(
    SettingsBluetoothSavedDevicesSubpageElement.is,
    SettingsBluetoothSavedDevicesSubpageElement);
