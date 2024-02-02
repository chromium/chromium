// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Settings subpage for managing Bluetooth saved devices.
 */

import '../settings_shared.css.js';
import './os_saved_devices_list.js';

import {FastPairSavedDevicesUiEvent, recordSavedDevicesUiEventMetrics} from 'chrome://resources/ash/common/bluetooth/bluetooth_metrics_utils.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {OsSettingsSubpageElement} from '../os_settings_page/os_settings_subpage.js';
import {Route, routes} from '../router.js';

import {OsBluetoothDevicesSubpageBrowserProxy, OsBluetoothDevicesSubpageBrowserProxyImpl} from './os_bluetooth_devices_subpage_browser_proxy.js';
import {getTemplate} from './os_bluetooth_saved_devices_subpage.html.js';
import {FastPairSavedDevice, FastPairSavedDevicesOptInStatus} from './settings_fast_pair_constants.js';

const SettingsBluetoothSavedDevicesSubpageElementBase =
    RouteObserverMixin(WebUiListenerMixin(I18nMixin(PolymerElement)));

export class SettingsBluetoothSavedDevicesSubpageElement extends
    SettingsBluetoothSavedDevicesSubpageElementBase {
  static get is() {
    return 'os-settings-bluetooth-saved-devices-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      savedDevices_: {
        type: Array,
        value: [],
      },

      showSavedDevicesErrorLabel_: {
        type: Boolean,
        value: false,
      },

      showSavedDevicesLoadingLabel_: {
        type: Boolean,
        notify: true,
        value: true,
      },

      shouldShowDeviceList_: {
        type: Boolean,
        value: false,
      },

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

  private browserProxy_: OsBluetoothDevicesSubpageBrowserProxy;
  private savedDevices_: FastPairSavedDevice[];
  private shouldShowDeviceList_: boolean;
  private shouldShowNoDevicesLabel_: boolean;
  private showSavedDevicesErrorLabel_: boolean;
  private showSavedDevicesLoadingLabel_: boolean;

  constructor() {
    super();

    this.browserProxy_ =
        OsBluetoothDevicesSubpageBrowserProxyImpl.getInstance();
  }

  override ready(): void {
    super.ready();
    this.addWebUiListener(
        'fast-pair-saved-devices-list', this.getSavedDevices_.bind(this));
    this.addWebUiListener(
        'fast-pair-saved-devices-opt-in-status',
        this.getOptInStatus_.bind(this));
    recordSavedDevicesUiEventMetrics(
        FastPairSavedDevicesUiEvent.SETTINGS_SAVED_DEVICE_LIST_SUBPAGE_SHOWN);
  }

  private getSavedDevices_(devices: FastPairSavedDevice[]): void {
    this.savedDevices_ = devices.slice(0);

    if (this.savedDevices_.length > 0) {
      recordSavedDevicesUiEventMetrics(
          FastPairSavedDevicesUiEvent.SETTINGS_SAVED_DEVICE_LIST_HAS_DEVICES);
    }
  }

  private getOptInStatus_(optInStatus: FastPairSavedDevicesOptInStatus): void {
    if (optInStatus ===
        FastPairSavedDevicesOptInStatus
            .STATUS_ERROR_RETRIEVING_FROM_FOOTPRINTS_SERVER) {
      this.showSavedDevicesErrorLabel_ = true;
    }
    this.showSavedDevicesLoadingLabel_ = false;
  }

  /**
   * RouteObserverMixin override
   */
  override currentRouteChanged(route: Route): void {
    // If we're navigating to the Saved Devices page, fetch the devices.
    if (route === routes.BLUETOOTH_SAVED_DEVICES) {
      this.showSavedDevicesErrorLabel_ = false;
      this.showSavedDevicesLoadingLabel_ = true;
      (this.parentNode as OsSettingsSubpageElement).pageTitle =
          loadTimeData.getString('savedDevicesPageName');
      this.browserProxy_.requestFastPairSavedDevices();
    }
  }

  private evaluateLabels_(): void {
    this.shouldShowDeviceList_ =
        !this.showSavedDevicesLoadingLabel_ && this.savedDevices_.length > 0;
    this.shouldShowNoDevicesLabel_ = !this.showSavedDevicesLoadingLabel_ &&
        !this.showSavedDevicesErrorLabel_ && this.savedDevices_.length === 0;
  }

  private computeSavedDevicesSublabel_(): string {
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

declare global {
  interface HTMLElementTagNameMap {
    [SettingsBluetoothSavedDevicesSubpageElement.is]:
        SettingsBluetoothSavedDevicesSubpageElement;
  }
}

customElements.define(
    SettingsBluetoothSavedDevicesSubpageElement.is,
    SettingsBluetoothSavedDevicesSubpageElement);
