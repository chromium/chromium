// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Settings subpage for managing Bluetooth properties and devices.
 */

import '../settings_shared.css.js';
import './os_paired_bluetooth_list.js';
import './settings_fast_pair_toggle.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {BluetoothUiSurface, recordBluetoothUiSurfaceMetrics} from 'chrome://resources/ash/common/bluetooth/bluetooth_metrics_utils.js';
import {getBluetoothConfig} from 'chrome://resources/ash/common/bluetooth/cros_bluetooth_config.js';
import {getHidPreservingController} from 'chrome://resources/ash/common/bluetooth/hid_preserving_bluetooth_state_controller.js';
import {HidWarningDialogSource} from 'chrome://resources/ash/common/bluetooth/hid_preserving_bluetooth_state_controller.mojom-webui.js';
import {getInstance as getAnnouncerInstance} from 'chrome://resources/ash/common/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {BluetoothSystemProperties, BluetoothSystemState, DeviceConnectionState, PairedBluetoothDeviceProperties} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {Route, Router, routes} from '../router.js';

import {getTemplate} from './os_bluetooth_devices_subpage.html.js';
import {OsBluetoothDevicesSubpageBrowserProxy, OsBluetoothDevicesSubpageBrowserProxyImpl} from './os_bluetooth_devices_subpage_browser_proxy.js';

const SettingsBluetoothDevicesSubpageElementBase = DeepLinkingMixin(PrefsMixin(
    RouteObserverMixin(WebUiListenerMixin(I18nMixin(PolymerElement)))));

export class SettingsBluetoothDevicesSubpageElement extends
    SettingsBluetoothDevicesSubpageElementBase {
  static get is() {
    return 'os-settings-bluetooth-devices-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      systemProperties: {
        type: Object,
        observer: 'onSystemPropertiesChanged_',
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () =>
            new Set<Setting>([Setting.kBluetoothOnOff, Setting.kFastPairOnOff]),
      },

      /**
       * Reflects the current state of the toggle button. This will be set when
       * the |systemProperties| state changes or when the user presses the
       * toggle.
       */
      isBluetoothToggleOn_: {
        type: Boolean,
        observer: 'onIsBluetoothToggleOnChanged_',
      },

      /**
       * Whether or not this device has the requirements to support fast pair.
       */
      isFastPairSupportedByDevice_: {
        type: Boolean,
        value: true,
      },

      connectedDevices_: {
        type: Array,
        value: [],
      },

      savedDevicesSublabel_: {
        type: String,
        value() {
          return loadTimeData.getString('sublabelWithEmail');
        },
      },

      unconnectedDevices_: {
        type: Array,
        value: [],
      },

      isBluetoothDisconnectWarningEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('bluetoothDisconnectWarningFlag');
        },
        readOnly: true,
      },
    };
  }

  systemProperties: BluetoothSystemProperties;
  private browserProxy_: OsBluetoothDevicesSubpageBrowserProxy;
  private connectedDevices_: PairedBluetoothDeviceProperties[];
  private isBluetoothToggleOn_: boolean;
  private isFastPairSupportedByDevice_: boolean;
  private lastSelectedDeviceId_: string|null;
  private savedDevicesSublabel_: string;
  private unconnectedDevices_: PairedBluetoothDeviceProperties[];
  private isBluetoothDisconnectWarningEnabled_: boolean;

  constructor() {
    super();

    /**
     * The id of the last device that was selected to view its detail page.
     */
    this.lastSelectedDeviceId_ = null;

    this.browserProxy_ =
        OsBluetoothDevicesSubpageBrowserProxyImpl.getInstance();
  }

  override ready(): void {
    super.ready();

    if (loadTimeData.getBoolean('enableFastPairFlag')) {
      this.addWebUiListener(
          'fast-pair-device-supported-status', (isSupported: boolean) => {
            this.isFastPairSupportedByDevice_ = isSupported;
          });
      this.browserProxy_.requestFastPairDeviceSupport();
    }
  }

  /**
   * RouteObserverMixin override
   */
  override currentRouteChanged(route: Route, oldRoute?: Route): void {
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
    this.browserProxy_.showBluetoothRevampHatsSurvey();

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

  private onSystemPropertiesChanged_(): void {
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

  private focusLastSelectedDeviceItem_(): void {
    const focusItem = (deviceListSelector: string, index: number): void => {
      const deviceList =
          this.shadowRoot!.querySelector<HTMLElement>(deviceListSelector);
      const items = deviceList!.shadowRoot!.querySelectorAll(
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
   */
  private onIsBluetoothToggleOnChanged_(_newValue: boolean, oldValue?: boolean):
      void {
    if (oldValue === undefined) {
      return;
    }

    this.announceBluetoothStateChange_();
  }

  private isToggleDisabled_(): boolean {
    // TODO(crbug.com/1010321): Add check for modification state when variable
    // is available.
    return this.systemProperties.systemState ===
        BluetoothSystemState.kUnavailable;
  }

  private getOnOffString_(
      isBluetoothToggleOn: boolean, onString: string,
      offString: string): string {
    return isBluetoothToggleOn ? onString : offString;
  }

  private shouldShowDeviceList_(devices: PairedBluetoothDeviceProperties[]):
      boolean {
    return devices.length > 0;
  }

  private shouldShowNoDevicesFound_(): boolean {
    return !this.connectedDevices_.length && !this.unconnectedDevices_.length;
  }

  private announceBluetoothStateChange_(): void {
    getAnnouncerInstance().announce(
        this.isBluetoothToggleOn_ ? this.i18n('bluetoothEnabledA11YLabel') :
                                    this.i18n('bluetoothDisabledA11YLabel'));
  }

  private isFastPairToggleVisible_(): boolean {
    return this.isFastPairSupportedByDevice_ &&
        loadTimeData.getBoolean('enableFastPairFlag');
  }

  private onBluetoothToggleChange_(event: CustomEvent): void {
    event.stopPropagation();

    // If the toggle value changed but the toggle is disabled, the change came
    // from CrosBluetoothConfig, not the user. Don't attempt to update the
    // enabled state.
    if (this.isToggleDisabled_()) {
      return;
    }

    const enabled = event.detail;
    if (this.isBluetoothDisconnectWarningEnabled_) {
      // Reset Bluetooth toggle state to previous state. Toggle should only be
      // updated when System properties changes.
      this.isBluetoothToggleOn_ = !enabled;
      getHidPreservingController().tryToSetBluetoothEnabledState(
          enabled, HidWarningDialogSource.kOsSettings);
    } else {
      getBluetoothConfig().setBluetoothEnabledState(enabled);
    }
  }

  /**
   * Determines if we allow access to the Saved Devices page. Unlike the Fast
   * Pair toggle, the device does not need to support Fast Pair because a device
   * could be saved to the user's account from a different device but managed on
   * this device. However Fast Pair must be enabled to confirm we have all Fast
   * Pair (and Saved Device) related code working on the device.
   */
  private isFastPairSavedDevicesRowVisible_(): boolean {
    return loadTimeData.getBoolean('enableFastPairFlag') &&
        loadTimeData.getBoolean('enableSavedDevicesFlag') &&
        !loadTimeData.getBoolean('isGuest') &&
        loadTimeData.getBoolean('isCrossDeviceFeatureSuiteEnabled');
  }

  private onClicked_(event: Event): void {
    Router.getInstance().navigateTo(routes.BLUETOOTH_SAVED_DEVICES);
    event.stopPropagation();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsBluetoothDevicesSubpageElement.is]:
        SettingsBluetoothDevicesSubpageElement;
  }
}

customElements.define(
    SettingsBluetoothDevicesSubpageElement.is,
    SettingsBluetoothDevicesSubpageElement);
