// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Settings subpage for managing Bluetooth properties and devices.
 */

import '../../settings_shared.css.js';
import './os_paired_bluetooth_list.js';
import './settings_fast_pair_toggle.js';

import {BluetoothUiSurface, recordBluetoothUiSurfaceMetrics} from 'chrome://resources/ash/common/bluetooth/bluetooth_metrics_utils.js';
import {getBluetoothConfig} from 'chrome://resources/ash/common/bluetooth/cros_bluetooth_config.js';
import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/ash/common/web_ui_listener_behavior.js';
import {BluetoothSystemProperties, BluetoothSystemState, DeviceConnectionState, PairedBluetoothDeviceProperties} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {Route, Router} from '../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

import {OsBluetoothDevicesSubpageBrowserProxy, OsBluetoothDevicesSubpageBrowserProxyImpl} from './os_bluetooth_devices_subpage_browser_proxy.js';

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
      I18nBehavior,
      RouteObserverBehavior,
      DeepLinkingBehavior,
      WebUIListenerBehavior,
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
       * @type {!BluetoothSystemProperties}
       */
      systemProperties: {
        type: Object,
        observer: 'onSystemPropertiesChanged_',
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([Setting.kBluetoothOnOff, Setting.kFastPairOnOff]),
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
       * @private {!Array<!PairedBluetoothDeviceProperties>}
       */
      connectedDevices_: {
        type: Array,
        value: [],
      },

      /**
       * @private
       */
      savedDevicesSublabel_: {
        type: String,
        value() {
          return loadTimeData.getString('sublabelWithEmail');
        },
      },

      /**
       * @private {!Array<!PairedBluetoothDeviceProperties>}
       */
      unconnectedDevices_: {
        type: Array,
        value: [],
      },
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
    this.isBluetoothToggleOn_ =
        this.systemProperties.systemState === BluetoothSystemState.kEnabled ||
        this.systemProperties.systemState === BluetoothSystemState.kEnabling;

    this.connectedDevices_ = this.systemProperties.pairedDevices.filter(
        device => device.deviceProperties.connectionState ===
            DeviceConnectionState.kConnected);
    this.unconnectedDevices_ = this.systemProperties.pairedDevices.filter(
        device => device.deviceProperties.connectionState !==
            DeviceConnectionState.kConnected);
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
    // If the toggle value changed but the toggle is disabled, the change came
    // from CrosBluetoothConfig, not the user. Don't attempt to update the
    // enabled state.
    if (!this.isToggleDisabled_()) {
      getBluetoothConfig().setBluetoothEnabledState(this.isBluetoothToggleOn_);
    }
    this.announceBluetoothStateChange_();
  }

  /**
   * @return {boolean}
   * @private
   */
  isToggleDisabled_() {
    // TODO(crbug.com/1010321): Add check for modification state when variable
    // is available.
    return this.systemProperties.systemState ===
        BluetoothSystemState.kUnavailable;
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
   * @param {!Array<!PairedBluetoothDeviceProperties>}
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
  announceBluetoothStateChange_() {
    getAnnouncerInstance().announce(
        this.isBluetoothToggleOn_ ? this.i18n('bluetoothEnabledA11YLabel') :
                                    this.i18n('bluetoothDisabledA11YLabel'));
  }

  /**
   * @return {boolean}
   * @private
   */
  isFastPairToggleVisible_() {
    return this.isFastPairSupportedByDevice_ &&
        loadTimeData.getBoolean('enableFastPairFlag');
  }

  /**
   * Determines if we allow access to the Saved Devices page. Unlike the Fast
   * Pair toggle, the device does not need to support Fast Pair because a device
   * could be saved to the user's account from a different device but managed on
   * this device. However Fast Pair must be enabled to confirm we have all Fast
   * Pair (and Saved Device) related code working on the device.
   * @return {boolean}
   * @private
   */
  isFastPairSavedDevicesRowVisible_() {
    return loadTimeData.getBoolean('enableFastPairFlag') &&
        loadTimeData.getBoolean('enableSavedDevicesFlag') &&
        !loadTimeData.getBoolean('isGuest');
  }

  /**
   * @param {!Event} event
   * @private
   */
  onClicked_(event) {
    Router.getInstance().navigateTo(routes.BLUETOOTH_SAVED_DEVICES);
    event.stopPropagation();
  }
}

customElements.define(
    SettingsBluetoothDevicesSubpageElement.is,
    SettingsBluetoothDevicesSubpageElement);
