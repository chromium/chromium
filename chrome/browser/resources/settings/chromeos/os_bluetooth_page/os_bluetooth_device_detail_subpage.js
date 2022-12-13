// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Settings subpage for managing Bluetooth device detail. This Element should
 * only be called when a device exist.
 */

import '../../settings_shared.css.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/policy/cr_tooltip_icon.js';
import './os_bluetooth_change_device_name_dialog.js';
import './os_bluetooth_true_wireless_images.js';
import 'chrome://resources/ash/common/bluetooth/bluetooth_device_battery_info.js';

import {BluetoothUiSurface, recordBluetoothUiSurfaceMetrics} from 'chrome://resources/ash/common/bluetooth/bluetooth_metrics_utils.js';
import {BatteryType} from 'chrome://resources/ash/common/bluetooth/bluetooth_types.js';
import {getBatteryPercentage, getDeviceName, hasAnyDetailedBatteryInfo, hasDefaultImage, hasTrueWirelessImages} from 'chrome://resources/ash/common/bluetooth/bluetooth_utils.js';
import {getBluetoothConfig} from 'chrome://resources/ash/common/bluetooth/cros_bluetooth_config.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/ash/common/web_ui_listener_behavior.js';
import {assertNotReached} from 'chrome://resources/ash/common/assert.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {AudioOutputCapability, BluetoothSystemProperties, DeviceConnectionState, DeviceType, PairedBluetoothDeviceProperties} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route, Router} from '../router.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';
import {RouteOriginBehavior, RouteOriginBehaviorInterface} from '../route_origin_behavior.js';

/** @enum {number} */
const PageState = {
  DISCONNECTED: 1,
  DISCONNECTING: 2,
  CONNECTING: 3,
  CONNECTED: 4,
  CONNECTION_FAILED: 5,
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {RouteObserverBehaviorInterface}
 * @implements {RouteOriginBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsBluetoothDeviceDetailSubpageElementBase = mixinBehaviors(
    [
      RouteObserverBehavior,
      RouteOriginBehavior,
      I18nBehavior,
      WebUIListenerBehavior,
    ],
    PolymerElement);

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
       * @type {!BluetoothSystemProperties}
       */
      systemProperties: {
        type: Object,
      },

      /**
       * @private {?PairedBluetoothDeviceProperties}
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

      /** @private */
      shouldShowChangeDeviceNameDialog_: {
        type: Boolean,
        value: false,
      },

      /** @private {!PageState} */
      pageState_: {
        type: Object,
        value: PageState.DISCONNECTED,
      },

      /** @protected */
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

  constructor() {
    super();

    /** RouteOriginBehaviorInterface override */
    this.route_ = routes.BLUETOOTH_DEVICE_DETAIL;
  }

  /** @override */
  ready() {
    super.ready();

    this.addEventListener(
        'forget-bluetooth-device', this.forgetDeviceConfirmed_);

    this.addFocusConfig(routes.POINTERS, '#changeMouseSettings');
    this.addFocusConfig(routes.KEYBOARD, '#changeKeyboardSettings');
  }

  /**
   * RouteObserverBehaviorInterface override
   * @param {!Route} route
   * @param {!Route=} opt_oldRoute
   */
  currentRouteChanged(route, opt_oldRoute) {
    super.currentRouteChanged(route, opt_oldRoute);

    if (route !== this.route_) {
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
  }

  /** @private */
  onSystemPropertiesOrDeviceIdChanged_() {
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

  /**
   * @return {boolean}
   * @private
   */
  computeIsDeviceConnected_() {
    if (!this.device_) {
      return false;
    }
    return this.device_.deviceProperties.connectionState ===
        DeviceConnectionState.kConnected;
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
   * @return {string}
   * @private
   */
  getBluetoothConnectDisconnectBtnLabel_() {
    return this.isDeviceConnected_ ? this.i18n('bluetoothDisconnect') :
                                     this.i18n('bluetoothConnect');
  }

  /**
   * @return {string}
   * @private
   */
  getBluetoothStateTextLabel_() {
    if (this.pageState_ === PageState.CONNECTING) {
      return this.i18n('bluetoothConnecting');
    }

    return this.pageState_ === PageState.CONNECTED ?
        this.i18n('bluetoothDeviceDetailConnected') :
        this.i18n('bluetoothDeviceDetailDisconnected');
  }

  /**
   * @return {string}
   * @private
   */
  getDeviceName_() {
    if (!this.device_) {
      return '';
    }
    return getDeviceName(this.device_);
  }

  /**
   * @return {boolean}
   * @private
   */
  shouldShowConnectDisconnectBtn_() {
    if (!this.device_) {
      return false;
    }
    return this.device_.deviceProperties.audioCapability ===
        AudioOutputCapability.kCapableOfAudioOutput;
  }

  /**
   * @return {boolean}
   * @private
   */
  shouldShowForgetBtn_() {
    return !!this.device_;
  }

  /** @private */
  onDeviceChanged_() {
    if (!this.device_) {
      return;
    }
    this.parentNode.pageTitle = getDeviceName(this.device_);

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

  /**
   * @return {boolean}
   * @private
   */
  shouldShowNonAudioOutputDeviceMessage_() {
    if (!this.device_) {
      return false;
    }
    return this.device_.deviceProperties.audioCapability !==
        AudioOutputCapability.kCapableOfAudioOutput;
  }

  /**
   * Message displayed for devices that are human interactive.
   * @return {string}
   * @private
   */
  getNonAudioOutputDeviceMessage_() {
    if (!this.device_) {
      return '';
    }

    if (this.device_.deviceProperties.connectionState ===
        DeviceConnectionState.kConnected) {
      return this.i18n('bluetoothDeviceDetailHIDMessageConnected');
    }

    return this.i18n('bluetoothDeviceDetailHIDMessageDisconnected');
  }

  /** @private */
  onChangeNameClick_() {
    this.shouldShowChangeDeviceNameDialog_ = true;
  }

  /** @private */
  onCloseChangeDeviceNameDialog_() {
    this.shouldShowChangeDeviceNameDialog_ = false;
  }

  /**
   * @return {string}
   * @private
   */
  getChangeDeviceNameBtnA11yLabel_() {
    if (!this.device_) {
      return '';
    }

    return this.i18n(
        'bluetoothDeviceDetailChangeDeviceNameBtnA11yLabel',
        this.getDeviceName_());
  }

  /**
   * @return {string}
   * @private
   */
  getMultipleBatteryInfoA11yLabel_() {
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

  /**
   * @return {string}
   * @private
   */
  getBatteryInfoA11yLabel_() {
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

  /**
   * @return {string}
   * @private
   */
  getDeviceStatusA11yLabel_() {
    if (!this.device_) {
      return '';
    }

    switch (this.pageState_) {
      case PageState.CONNECTING:
        return this.i18n(
            'bluetoothDeviceDetailConnectingA11yLabel', this.getDeviceName_());
      case PageState.CONNECTED:
        return this.i18n(
            'bluetoothDeviceDetailConnectedA11yLabel', this.getDeviceName_());
      case PageState.CONNECTION_FAILED:
        return this.i18n(
            'bluetoothDeviceDetailConnectionFailureA11yLabel',
            this.getDeviceName_());
      case PageState.DISCONNECTED:
      case PageState.DISCONNECTING:
        return this.i18n(
            'bluetoothDeviceDetailDisconnectedA11yLabel',
            this.getDeviceName_());
      default:
        assertNotReached();
    }
  }

  /**
   * @return {boolean}
   * @private
   */
  shouldShowChangeMouseDeviceSettings_() {
    if (!this.device_ || !this.isDeviceConnected_) {
      return false;
    }
    return this.device_.deviceProperties.deviceType === DeviceType.kMouse ||
        this.device_.deviceProperties.deviceType ===
        DeviceType.kKeyboardMouseCombo;
  }

  /**
   * @return {boolean}
   * @private
   */
  shouldShowChangeKeyboardDeviceSettings_() {
    if (!this.device_ || !this.isDeviceConnected_) {
      return false;
    }
    return this.device_.deviceProperties.deviceType === DeviceType.kKeyboard ||
        this.device_.deviceProperties.deviceType ===
        DeviceType.kKeyboardMouseCombo;
  }

  /**
   * @return {boolean}
   * @private
   */
  shouldShowBlockedByPolicyIcon_() {
    if (!this.device_) {
      return false;
    }

    return this.device_.deviceProperties.isBlockedByPolicy;
  }

  /**
   * @return {boolean}
   * @private
   */
  shouldShowBatteryInfo_() {
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

  /**
   * @return {boolean}
   * @private
   */
  shouldShowTrueWirelessImages_() {
    if (!this.device_) {
      return false;
    }

    // The True Wireless Images component expects all True Wireless
    // images and the default image to be displayable.
    if (!hasDefaultImage(this.device_.deviceProperties) ||
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

  /**
   * @param {!Event} event
   * @private
   */
  onConnectDisconnectBtnClick_(event) {
    event.stopPropagation();
    if (this.pageState_ === PageState.DISCONNECTED ||
        this.pageState_ === PageState.CONNECTION_FAILED) {
      this.connectDevice_();
      return;
    }
    this.disconnectDevice_();
  }

  /** @private */
  connectDevice_() {
    this.pageState_ = PageState.CONNECTING;
    getBluetoothConfig().connect(this.deviceId_).then(response => {
      this.handleConnectResult_(response.success);
    });
  }

  /**
   * @param {boolean} success
   * @private
   */
  handleConnectResult_(success) {
    this.pageState_ =
        success ? PageState.CONNECTED : PageState.CONNECTION_FAILED;
  }

  /** @private */
  disconnectDevice_() {
    this.pageState_ = PageState.DISCONNECTING;
    getBluetoothConfig().disconnect(this.deviceId_).then(response => {
      this.handleDisconnectResult_(response.success);
    });
  }

  /**
   * @param {boolean} success
   * @private
   */
  handleDisconnectResult_(success) {
    if (success) {
      this.pageState_ = PageState.DISCONNECTED;
    }
  }

  /**
   * @return {boolean}
   * @private
   */
  isConnectDisconnectBtnDisabled() {
    return this.pageState_ === PageState.CONNECTING ||
        this.pageState_ === PageState.DISCONNECTING;
  }

  /**
   * @return {boolean}
   * @private
   */
  shouldShowErrorMessage_() {
    return this.pageState_ === PageState.CONNECTION_FAILED;
  }

  /**
   * @return {?PairedBluetoothDeviceProperties}
   */
  getDeviceForTest() {
    return this.device_;
  }

  /**
   * @return {string}
   */
  getDeviceIdForTest() {
    return this.deviceId_;
  }

  /** @return {boolean} */
  getIsDeviceConnectedForTest() {
    return this.isDeviceConnected_;
  }

  /** @private */
  onMouseRowClick_() {
    Router.getInstance().navigateTo(routes.POINTERS);
  }

  /** @private */
  onKeyboardRowClick_() {
    Router.getInstance().navigateTo(routes.KEYBOARD);
  }

  /**
   * @return {string}
   * @private
   */
  getForgetA11yLabel_() {
    return this.i18n(
        'bluetoothDeviceDetailForgetA11yLabel', this.getDeviceName_());
  }

  /** @private */
  onForgetButtonClicked_() {
    if (loadTimeData.getBoolean('enableFastPairFlag')) {
      this.shouldShowForgetDeviceDialog_ = true;
    } else {
      getBluetoothConfig().forget(this.deviceId_);
    }
  }

  /** @private */
  onCloseForgetDeviceDialog_() {
    this.shouldShowForgetDeviceDialog_ = false;
  }

  /** @private */
  forgetDeviceConfirmed_() {
    getBluetoothConfig().forget(this.deviceId_);
  }
}

customElements.define(
    SettingsBluetoothDeviceDetailSubpageElement.is,
    SettingsBluetoothDeviceDetailSubpageElement);
