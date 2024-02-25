// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Settings subpage for managing Bluetooth device detail. This Element should
 * only be called when a device exist.
 */

import '../settings_shared.css.js';
import 'chrome://resources/ash/common/bluetooth/bluetooth_icon.js';
import 'chrome://resources/ash/common/cr_elements/policy/cr_tooltip_icon.js';
import './os_bluetooth_change_device_name_dialog.js';
import './os_bluetooth_true_wireless_images.js';
import 'chrome://resources/ash/common/bluetooth/bluetooth_device_battery_info.js';

import {BluetoothUiSurface, recordBluetoothUiSurfaceMetrics} from 'chrome://resources/ash/common/bluetooth/bluetooth_metrics_utils.js';
import {BatteryType} from 'chrome://resources/ash/common/bluetooth/bluetooth_types.js';
import {getBatteryPercentage, getDeviceNameUnsafe, hasAnyDetailedBatteryInfo, hasDefaultImage, hasTrueWirelessImages} from 'chrome://resources/ash/common/bluetooth/bluetooth_utils.js';
import {getBluetoothConfig} from 'chrome://resources/ash/common/bluetooth/cros_bluetooth_config.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {AudioOutputCapability, BluetoothSystemProperties, DeviceConnectionState, DeviceType, PairedBluetoothDeviceProperties} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isInputDeviceSettingsSplitEnabled} from '../common/load_time_booleans.js';
import {RouteOriginMixin} from '../common/route_origin_mixin.js';
import {OsSettingsSubpageElement} from '../os_settings_page/os_settings_subpage.js';
import {Route, Router, routes} from '../router.js';

import {getTemplate} from './os_bluetooth_device_detail_subpage.html.js';
import {OsBluetoothDevicesSubpageBrowserProxy, OsBluetoothDevicesSubpageBrowserProxyImpl} from './os_bluetooth_devices_subpage_browser_proxy.js';

enum PageState {
  DISCONNECTED = 1,
  DISCONNECTING = 2,
  CONNECTING = 3,
  CONNECTED = 4,
  CONNECTION_FAILED = 5
}

const SettingsBluetoothDeviceDetailSubpageElementBase =
    RouteOriginMixin(WebUiListenerMixin(I18nMixin((PolymerElement))));

export class SettingsBluetoothDeviceDetailSubpageElement extends
    SettingsBluetoothDeviceDetailSubpageElementBase {
  static get is() {
    return 'os-settings-bluetooth-device-detail-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      systemProperties: {
        type: Object,
      },

      device_: {
        type: Object,
        observer: 'onDeviceChanged_',
      },

      /**
       * Id of the currently paired device. This is set from the route query
       * parameters.
       */
      deviceId_: {
        type: String,
        value: '',
      },

      isDeviceConnected_: {
        reflectToAttribute: true,
        type: Boolean,
        computed: 'computeIsDeviceConnected_(device_.*)',
      },

      shouldShowChangeDeviceNameDialog_: {
        type: Boolean,
        value: false,
      },

      pageState_: {
        type: Object,
        value: PageState.DISCONNECTED,
      },

      shouldShowForgetDeviceDialog_: {
        type: Boolean,
        value: false,
      },
    };
  }

  static get observers() {
    return [
      'onSystemPropertiesOrDeviceIdChanged_(systemProperties.*, deviceId_)',
    ];
  }

  systemProperties: BluetoothSystemProperties;

  private browserProxy_: OsBluetoothDevicesSubpageBrowserProxy;
  private deviceId_: string;
  private device_: PairedBluetoothDeviceProperties|null;
  private isDeviceConnected_: boolean;
  private pageState_: PageState;
  private shouldShowChangeDeviceNameDialog_: boolean;
  private shouldShowForgetDeviceDialog_: boolean;

  constructor() {
    super();

    /** RouteOriginMixin override */
    this.route = routes.BLUETOOTH_DEVICE_DETAIL;

    this.browserProxy_ =
        OsBluetoothDevicesSubpageBrowserProxyImpl.getInstance();
  }

  override ready(): void {
    super.ready();

    this.addEventListener(
        'forget-bluetooth-device', this.forgetDeviceConfirmed_);

    if (isInputDeviceSettingsSplitEnabled()) {
      this.addFocusConfig(routes.PER_DEVICE_MOUSE, '#changeMouseSettings');
      this.addFocusConfig(
          routes.PER_DEVICE_KEYBOARD, '#changeKeyboardSettings');
    } else {
      this.addFocusConfig(routes.POINTERS, '#changeMouseSettings');
      this.addFocusConfig(routes.KEYBOARD, '#changeKeyboardSettings');
    }
  }

  override currentRouteChanged(route: Route, oldRoute?: Route): void {
    super.currentRouteChanged(route, oldRoute);

    if (route !== this.route) {
      return;
    }

    this.deviceId_ = '';
    this.pageState_ = PageState.DISCONNECTED;
    this.device_ = null;

    const queryParams = Router.getInstance().getQueryParameters();
    const deviceId = queryParams.get('id') || '';
    if (!deviceId) {
      console.error('No id specified for page:' + route);
      return;
    }
    this.deviceId_ = decodeURIComponent(deviceId);
    recordBluetoothUiSurfaceMetrics(
        BluetoothUiSurface.SETTINGS_DEVICE_DETAIL_SUBPAGE);
    this.browserProxy_.showBluetoothRevampHatsSurvey();
  }

  private onSystemPropertiesOrDeviceIdChanged_(): void {
    if (!this.systemProperties || !this.deviceId_) {
      return;
    }

    this.device_ =
        this.systemProperties.pairedDevices.find(
            (device) => device.deviceProperties.id === this.deviceId_) ||
        null;

    if (this.device_ ||
        Router.getInstance().currentRoute !== routes.BLUETOOTH_DEVICE_DETAIL) {
      return;
    }

    // Special case where the device was turned off or becomes unavailable
    // while user is vewing the page, return back to previous page.
    this.deviceId_ = '';
    Router.getInstance().navigateToPreviousRoute();
  }

  private computeIsDeviceConnected_(): boolean {
    if (!this.device_) {
      return false;
    }
    return this.device_.deviceProperties.connectionState ===
        DeviceConnectionState.kConnected;
  }

  private getBluetoothConnectDisconnectBtnLabel_(): string {
    return this.isDeviceConnected_ ? this.i18n('bluetoothDisconnect') :
                                     this.i18n('bluetoothConnect');
  }

  private getBluetoothStateTextLabel_(): string {
    if (this.pageState_ === PageState.CONNECTING) {
      return this.i18n('bluetoothConnecting');
    }

    if (this.pageState_ === PageState.DISCONNECTING) {
      return this.i18n('bluetoothDeviceDetailConnected');
    }

    return this.pageState_ === PageState.CONNECTED ?
        this.i18n('bluetoothDeviceDetailConnected') :
        this.i18n('bluetoothDeviceDetailDisconnected');
  }

  private getDeviceNameUnsafe_(): string {
    return getDeviceNameUnsafe(this.device_);
  }

  private shouldShowConnectDisconnectBtn_(): boolean {
    if (!this.device_) {
      return false;
    }
    return this.device_.deviceProperties.audioCapability ===
        AudioOutputCapability.kCapableOfAudioOutput;
  }

  private shouldShowForgetBtn_(): boolean {
    return !!this.device_;
  }

  private onDeviceChanged_(): void {
    if (!this.device_) {
      return;
    }
    (this.parentNode as OsSettingsSubpageElement).pageTitle =
        getDeviceNameUnsafe(this.device_);

    // Special case a where user is still on detail page and has
    // tried to connect to device but failed. The current |pageState_|
    // is CONNECTION_FAILED, but another device property not
    // |connectionState| has changed.
    if (this.pageState_ === PageState.CONNECTION_FAILED &&
        this.device_.deviceProperties.connectionState ===
            DeviceConnectionState.kNotConnected) {
      return;
    }

    switch (this.device_.deviceProperties.connectionState) {
      case DeviceConnectionState.kConnected:
        this.pageState_ = PageState.CONNECTED;
        break;
      case DeviceConnectionState.kNotConnected:
        this.pageState_ = PageState.DISCONNECTED;
        break;
      case DeviceConnectionState.kConnecting:
        this.pageState_ = PageState.CONNECTING;
        break;
      default:
        assertNotReached();
    }
  }

  private shouldShowNonAudioOutputDeviceMessage_(): boolean {
    if (!this.device_) {
      return false;
    }
    return this.device_.deviceProperties.audioCapability !==
        AudioOutputCapability.kCapableOfAudioOutput;
  }

  /**
   * Message displayed for devices that are human interactive.
   */
  private getNonAudioOutputDeviceMessage_(): string {
    if (!this.device_) {
      return '';
    }

    if (this.device_.deviceProperties.connectionState ===
        DeviceConnectionState.kConnected) {
      return this.i18n('bluetoothDeviceDetailHIDMessageConnected');
    }

    return this.i18n('bluetoothDeviceDetailHIDMessageDisconnected');
  }

  private onChangeNameClick_(): void {
    this.shouldShowChangeDeviceNameDialog_ = true;
  }

  private onCloseChangeDeviceNameDialog_(): void {
    this.shouldShowChangeDeviceNameDialog_ = false;
  }

  private getChangeDeviceNameBtnA11yLabel_(): string {
    if (!this.device_) {
      return '';
    }

    return loadTimeData.getStringF(
        'bluetoothDeviceDetailChangeDeviceNameBtnA11yLabel',
        getDeviceNameUnsafe(this.device_));
  }

  private getMultipleBatteryInfoA11yLabel_(): string {
    assert(this.device_);
    let label = '';

    const leftBudBatteryPercentage = getBatteryPercentage(
        this.device_.deviceProperties, BatteryType.LEFT_BUD);
    if (leftBudBatteryPercentage !== undefined) {
      label = label +
          this.i18n(
              'bluetoothDeviceDetailLeftBudBatteryPercentageA11yLabel',
              leftBudBatteryPercentage);
    }

    const caseBatteryPercentage =
        getBatteryPercentage(this.device_.deviceProperties, BatteryType.CASE);
    if (caseBatteryPercentage !== undefined) {
      label = label +
          this.i18n(
              'bluetoothDeviceDetailCaseBatteryPercentageA11yLabel',
              caseBatteryPercentage);
    }

    const rightBudBatteryPercentage = getBatteryPercentage(
        this.device_.deviceProperties, BatteryType.RIGHT_BUD);
    if (rightBudBatteryPercentage !== undefined) {
      label = label +
          this.i18n(
              'bluetoothDeviceDetailRightBudBatteryPercentageA11yLabel',
              rightBudBatteryPercentage);
    }

    return label;
  }

  private getBatteryInfoA11yLabel_(): string {
    if (!this.device_) {
      return '';
    }

    if (hasAnyDetailedBatteryInfo(this.device_.deviceProperties)) {
      return this.getMultipleBatteryInfoA11yLabel_();
    }

    const batteryPercentage = getBatteryPercentage(
        this.device_.deviceProperties, BatteryType.DEFAULT);
    if (batteryPercentage === undefined) {
      return '';
    }
    return this.i18n(
        'bluetoothDeviceDetailBatteryPercentageA11yLabel', batteryPercentage);
  }

  private getDeviceStatusA11yLabel_(): string {
    if (!this.device_) {
      return '';
    }

    switch (this.pageState_) {
      case PageState.CONNECTING:
        return loadTimeData.getStringF(
            'bluetoothDeviceDetailConnectingA11yLabel',
            getDeviceNameUnsafe(this.device_));
      case PageState.CONNECTED:
        return loadTimeData.getStringF(
            'bluetoothDeviceDetailConnectedA11yLabel',
            getDeviceNameUnsafe(this.device_));
      case PageState.CONNECTION_FAILED:
        return loadTimeData.getStringF(
            'bluetoothDeviceDetailConnectionFailureA11yLabel',
            getDeviceNameUnsafe(this.device_));
      case PageState.DISCONNECTED:
      case PageState.DISCONNECTING:
        return loadTimeData.getStringF(
            'bluetoothDeviceDetailDisconnectedA11yLabel',
            getDeviceNameUnsafe(this.device_));
      default:
        assertNotReached();
    }
  }

  private shouldShowChangeMouseDeviceSettings_(): boolean {
    if (!this.device_ || !this.isDeviceConnected_) {
      return false;
    }
    return this.device_.deviceProperties.deviceType === DeviceType.kMouse ||
        this.device_.deviceProperties.deviceType ===
        DeviceType.kKeyboardMouseCombo;
  }

  private shouldShowChangeKeyboardDeviceSettings_(): boolean {
    if (!this.device_ || !this.isDeviceConnected_) {
      return false;
    }
    return this.device_.deviceProperties.deviceType === DeviceType.kKeyboard ||
        this.device_.deviceProperties.deviceType ===
        DeviceType.kKeyboardMouseCombo;
  }

  private shouldShowBlockedByPolicyIcon_(): boolean {
    if (!this.device_) {
      return false;
    }

    return this.device_.deviceProperties.isBlockedByPolicy;
  }

  private shouldShowBatteryInfo_(): boolean {
    if (!this.device_ || this.pageState_ === PageState.CONNECTING ||
        this.pageState_ === PageState.CONNECTION_FAILED) {
      return false;
    }

    // Don't show the inline Battery Info if we are showing the True
    // Wireless Images component.
    if (this.shouldShowTrueWirelessImages_()) {
      return false;
    }

    if (getBatteryPercentage(
            this.device_.deviceProperties, BatteryType.DEFAULT) !== undefined) {
      return true;
    }

    return hasAnyDetailedBatteryInfo(this.device_.deviceProperties);
  }

  private shouldShowTrueWirelessImages_(): boolean {
    if (!loadTimeData.getBoolean('enableFastPairFlag') || !this.device_) {
      return false;
    }

    // The True Wireless Images component expects either the True Wireless
    // images or the default image to be displayable.
    if (!hasDefaultImage(this.device_.deviceProperties) &&
        !hasTrueWirelessImages(this.device_.deviceProperties)) {
      return false;
    }

    // If the device is not connected, we don't need any battery info and can
    // immediately return true.
    if (!this.isDeviceConnected_) {
      return true;
    }

    // Don't show True Wireless Images component if the device is connected and
    // has no battery info to display.
    return getBatteryPercentage(
               this.device_.deviceProperties, BatteryType.DEFAULT) !==
        undefined ||
        hasAnyDetailedBatteryInfo(this.device_.deviceProperties);
  }

  private onConnectDisconnectBtnClick_(event: Event): void {
    event.stopPropagation();
    if (this.pageState_ === PageState.DISCONNECTED ||
        this.pageState_ === PageState.CONNECTION_FAILED) {
      this.connectDevice_();
      return;
    }
    this.disconnectDevice_();
  }

  private connectDevice_(): void {
    this.pageState_ = PageState.CONNECTING;
    getBluetoothConfig().connect(this.deviceId_).then(response => {
      this.handleConnectResult_(response.success);
    });
  }

  private handleConnectResult_(success: boolean): void {
    this.pageState_ =
        success ? PageState.CONNECTED : PageState.CONNECTION_FAILED;
  }

  private disconnectDevice_(): void {
    this.pageState_ = PageState.DISCONNECTING;

    // When disconnecting, disconnect() callback function could be called
    // a few seconds before device connectionState is updated. This
    // causes a situation where connectedState label is 'disconnected'
    // while the color is green. `pageState_` would be updated in
    // onDeviceChanged_().
    getBluetoothConfig().disconnect(this.deviceId_);
  }

  private isConnectDisconnectBtnDisabled(): boolean {
    return this.pageState_ === PageState.CONNECTING ||
        this.pageState_ === PageState.DISCONNECTING;
  }

  private shouldShowErrorMessage_(): boolean {
    return this.pageState_ === PageState.CONNECTION_FAILED;
  }

  getDeviceForTest(): PairedBluetoothDeviceProperties|null {
    return this.device_;
  }

  getDeviceIdForTest(): string {
    return this.deviceId_;
  }

  getIsDeviceConnectedForTest(): boolean {
    return this.isDeviceConnected_;
  }

  private onMouseRowClick_(): void {
    if (isInputDeviceSettingsSplitEnabled()) {
      Router.getInstance().navigateTo(routes.PER_DEVICE_MOUSE);
    } else {
      Router.getInstance().navigateTo(routes.POINTERS);
    }
  }

  private onKeyboardRowClick_(): void {
    if (isInputDeviceSettingsSplitEnabled()) {
      Router.getInstance().navigateTo(routes.PER_DEVICE_KEYBOARD);
    } else {
      Router.getInstance().navigateTo(routes.KEYBOARD);
    }
  }

  private getForgetA11yLabel_(): string {
    return loadTimeData.getStringF(
        'bluetoothDeviceDetailForgetA11yLabel',
        getDeviceNameUnsafe(this.device_));
  }

  private onForgetButtonClicked_(): void {
    if (loadTimeData.getBoolean('enableFastPairFlag')) {
      this.shouldShowForgetDeviceDialog_ = true;
    } else {
      getBluetoothConfig().forget(this.deviceId_);
    }
  }

  private onCloseForgetDeviceDialog_(): void {
    this.shouldShowForgetDeviceDialog_ = false;
  }

  private forgetDeviceConfirmed_(): void {
    getBluetoothConfig().forget(this.deviceId_);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsBluetoothDeviceDetailSubpageElement.is]:
        SettingsBluetoothDeviceDetailSubpageElement;
  }
  interface HTMLElementEventMap {
    'forget-bluetooth-device': Event;
  }
}

customElements.define(
    SettingsBluetoothDeviceDetailSubpageElement.is,
    SettingsBluetoothDeviceDetailSubpageElement);
