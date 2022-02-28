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
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from '//resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getBluetoothConfig} from 'chrome://resources/cr_components/chromeos/bluetooth/cros_bluetooth_config.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';

import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.m.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';
import {OsBluetoothDevicesSubpageBrowserProxy, OsBluetoothDevicesSubpageBrowserProxyImpl} from './os_bluetooth_devices_subpage_browser_proxy.js';

const mojom = chromeos.bluetoothConfig.mojom;

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsBluetoothDevicesSubpageElementBase = mixinBehaviors(
    [
      I18nBehavior, RouteObserverBehavior, DeepLinkingBehavior,
      WebUIListenerBehavior
    ],
    PolymerElement);

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
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!chromeos.settings.mojom.Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          chromeos.settings.mojom.Setting.kBluetoothOnOff,
          chromeos.settings.mojom.Setting.kFastPairOnOff
        ]),
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
       * Whether or not this device has the requirements to support fast pair.
       * @private {boolean}
       */
      isFastPairSupportedByDevice_: {
        type: Boolean,
        value: true,
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

  constructor() {
    super();

    /**
     * The id of the last device that was selected to view its detail page.
     * @private {?string}
     */
    this.lastSelectedDeviceId_ = null;

    /** @private {?OsBluetoothDevicesSubpageBrowserProxy} */
    this.browserProxy_ =
        OsBluetoothDevicesSubpageBrowserProxyImpl.getInstance();
  }

  /** @override */
  ready() {
    super.ready();
    IronA11yAnnouncer.requestAvailability();
    if (loadTimeData.getBoolean('enableFastPairFlag')) {
      this.addWebUIListener(
          'fast-pair-device-supported-status', (isSupported) => {
            this.isFastPairSupportedByDevice_ = isSupported;
          });
      this.browserProxy_.requestFastPairDeviceSupport();
    }
  }

  /**
   * RouteObserverBehaviorInterface override
   * @param {!Route} route
   * @param {!Route=} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // If we're navigating to a device's detail page, save the id of the device.
    if (route === routes.BLUETOOTH_DEVICE_DETAIL &&
        oldRoute === routes.BLUETOOTH_DEVICES) {
      const queryParams = Router.getInstance().getQueryParameters();
      this.lastSelectedDeviceId_ = queryParams.get('id');
      return;
    }

    if (route !== routes.BLUETOOTH_DEVICES) {
      return;
    }
    recordBluetoothUiSurfaceMetrics(
        BluetoothUiSurface.SETTINGS_DEVICE_LIST_SUBPAGE);

    this.attemptDeepLink();

    // If a backwards navigation occurred from a Bluetooth device's detail page,
    // focus the list item corresponding to that device.
    if (oldRoute !== routes.BLUETOOTH_DEVICE_DETAIL) {
      return;
    }

    // Don't attempt to focus any item unless the last navigation was a
    // 'pop' (backwards) navigation.
    if (!Router.getInstance().lastRouteChangeWasPopstate()) {
      return;
    }

    this.focusLastSelectedDeviceItem_();
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
        device => device.deviceProperties.connectionState ===
            mojom.DeviceConnectionState.kConnected);
    this.unconnectedDevices_ = this.systemProperties.pairedDevices.filter(
        device => device.deviceProperties.connectionState !==
            mojom.DeviceConnectionState.kConnected);
  }

  /** @private */
  focusLastSelectedDeviceItem_() {
    const focusItem = (deviceListSelector, index) => {
      const deviceList = this.shadowRoot.querySelector(deviceListSelector);
      const items = deviceList.shadowRoot.querySelectorAll(
          'os-settings-paired-bluetooth-list-item');
      if (index >= items.length) {
        return;
      }
      items[index].focus();
    };

    // Search |connectedDevices_| for the device.
    let index = this.connectedDevices_.findIndex(
        device => device.deviceProperties.id === this.lastSelectedDeviceId_);
    if (index >= 0) {
      focusItem(/*deviceListSelector=*/ '#connectedDeviceList', index);
      return;
    }

    // If |connectedDevices_| doesn't contain the device, search
    // |unconnectedDevices_|.
    index = this.unconnectedDevices_.findIndex(
        device => device.deviceProperties.id === this.lastSelectedDeviceId_);
    if (index < 0) {
      return;
    }
    focusItem(/*deviceListSelector=*/ '#unconnectedDeviceList', index);
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
    this.annouceBluetoothStateChange_();
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

  /** @private */
  annouceBluetoothStateChange_() {
    this.dispatchEvent(new CustomEvent('iron-announce', {
      bubbles: true,
      composed: true,
      detail: {
        text: this.isBluetoothToggleOn_ ?
            this.i18n('bluetoothEnabledA11YLabel') :
            this.i18n('bluetoothDisabledA11YLabel')
      }
    }));
  }

  /**
   * @return {boolean}
   * @private
   */
  isFastPairToggleVisible_() {
    return this.isFastPairSupportedByDevice_ &&
        loadTimeData.getBoolean('enableFastPairFlag');
  }
}

customElements.define(
    SettingsBluetoothDevicesSubpageElement.is,
    SettingsBluetoothDevicesSubpageElement);
