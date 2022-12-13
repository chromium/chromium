// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Settings subpage for managing Bluetooth saved devices.
 */

import '../../settings_shared.css.js';
import './os_saved_devices_list.js';

import {FastPairSavedDevicesUiEvent, recordSavedDevicesUiEventMetrics} from 'chrome://resources/ash/common/bluetooth/bluetooth_metrics_utils.js';
import {assertNotReached} from 'chrome://resources/ash/common/assert.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/ash/common/web_ui_listener_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route, Router} from '../router.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

import {OsBluetoothDevicesSubpageBrowserProxy, OsBluetoothDevicesSubpageBrowserProxyImpl} from './os_bluetooth_devices_subpage_browser_proxy.js';
import {FastPairSavedDevice, FastPairSavedDevicesOptInStatus} from './settings_fast_pair_constants.js';

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
       * @protected {!Array<!FastPairSavedDevice>}
       */
      savedDevices_: {
        type: Array,
        value: [],
      },

      /** @protected */
      showSavedDevicesErrorLabel_: {
        type: Boolean,
        value: false,
      },

      /** @protected */
      showSavedDevicesLoadingLabel_: {
        type: Boolean,
        notify: true,
        value: true,
      },

      /** @protected */
      shouldShowDeviceList_: {
        type: Boolean,
        value: false,
      },

      /** @protected */
      shouldShowNoDevicesLabel_: {
        type: Boolean,
        value: false,
      },
    };
  }

  static get observers() {
    return [
      'evaluateLabels_(showSavedDevicesLoadingLabel_, showSavedDevicesErrorLabel_, savedDevices_.*)',
    ];
  }

  constructor() {
    super();
    /** @private {?OsBluetoothDevicesSubpageBrowserProxy} */
    this.browserProxy_ =
        OsBluetoothDevicesSubpageBrowserProxyImpl.getInstance();
  }

  /** @override */
  ready() {
    super.ready();
    this.addWebUIListener(
        'fast-pair-saved-devices-list', this.getSavedDevices_.bind(this));
    this.addWebUIListener(
        'fast-pair-saved-devices-opt-in-status',
        this.getOptInStatus_.bind(this));
    recordSavedDevicesUiEventMetrics(
        FastPairSavedDevicesUiEvent.SETTINGS_SAVED_DEVICE_LIST_SUBPAGE_SHOWN);
  }

  /**
   * @param {!Array<!FastPairSavedDevice>} devices
   * @private
   */
  getSavedDevices_(devices) {
    this.savedDevices_ = devices.slice(0);

    if (this.savedDevices_.length > 0) {
      recordSavedDevicesUiEventMetrics(
          FastPairSavedDevicesUiEvent.SETTINGS_SAVED_DEVICE_LIST_HAS_DEVICES);
    }
  }

  /**
   * @private
   */
  getOptInStatus_(optInStatus) {
    if (optInStatus ===
        FastPairSavedDevicesOptInStatus
            .STATUS_ERROR_RETRIEVING_FROM_FOOTPRINTS_SERVER) {
      this.showSavedDevicesErrorLabel_ = true;
    }
    this.showSavedDevicesLoadingLabel_ = false;
  }

  /**
   * RouteObserverBehaviorInterface override
   * @param {!Route} route
   * @param {!Route=} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // If we're navigating to the Saved Devices page, fetch the devices.
    if (route === routes.BLUETOOTH_SAVED_DEVICES) {
      this.showSavedDevicesErrorLabel_ = false;
      this.showSavedDevicesLoadingLabel_ = true;
      this.parentNode.pageTitle =
          loadTimeData.getString('savedDevicesPageName');
      this.browserProxy_.requestFastPairSavedDevices();
      return;
    }
  }

  /** @private */
  evaluateLabels_() {
    this.shouldShowDeviceList_ =
        !this.showSavedDevicesLoadingLabel_ && this.savedDevices_.length > 0;
    this.shouldShowNoDevicesLabel_ = !this.showSavedDevicesLoadingLabel_ &&
        !this.showSavedDevicesErrorLabel_ && this.savedDevices_.length === 0;
  }

  /** @private */
  computeSavedDevicesSublabel_() {
    this.evaluateLabels_();
    if (this.shouldShowDeviceList_) {
      return loadTimeData.getString('sublabelWithEmail');
    }
    if (this.shouldShowNoDevicesLabel_) {
      return loadTimeData.getString('noDevicesWithEmail');
    }
    if (this.showSavedDevicesLoadingLabel_) {
      return loadTimeData.getString('loadingDevicesWithEmail');
    }
    if (this.showSavedDevicesErrorLabel_) {
      return loadTimeData.getString('savedDevicesErrorWithEmail');
    }
    assertNotReached();
  }
}

customElements.define(
    SettingsBluetoothSavedDevicesSubpageElement.is,
    SettingsBluetoothSavedDevicesSubpageElement);
