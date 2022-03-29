// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a bluetooth device in a list.
 */

import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import '//resources/cr_elements/icons.m.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../os_icons.js';
import '../../settings_shared_css.js';

import {FocusRowBehavior} from '//resources/js/cr/ui/focus_row_behavior.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BluetoothPageBrowserProxy, BluetoothPageBrowserProxyImpl} from './bluetooth_page_browser_proxy.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'bluetooth-device-list-item',

  behaviors: [I18nBehavior, FocusRowBehavior],

  properties: {
    /**
     * The bluetooth device.
     * @type {!chrome.bluetooth.Device}
     */
    device: {
      type: Object,
      observer: 'onDeviceChanged_',
    },

    /**
     * The aria-label attribute for the top-level bluetooth-device-list-item
     * element.
     */
    ariaLabel: {
      type: String,
      notify: true,
      computed: 'getAriaLabel_(device)',
    },

    // TODO(crbug.com/1208155) Add managed policy icon that is shown when this
    // flag is true.
    /** @private */
    shouldShowManagedIcon_: {
      type: Boolean,
      value: false,
    },
  },

  hostAttributes: {role: 'button'},

  /** @private {?BluetoothPageBrowserProxy} */
  browserProxy_: null,

  /** @override */
  ready() {
    this.browserProxy_ = BluetoothPageBrowserProxyImpl.getInstance();
  },

  /** @override */
  detached() {
    // Fire an event in case the tooltip was previously showing for the managed
    // icon in this item and this item is being removed.
    this.fireTooltipStateChangeEvent_(/*showTooltip=*/ false);
  },

  /**
   * @param {!Event} event
   * @private
   */
  ignoreEnterKey_(event) {
    if (event.key === 'Enter') {
      event.stopPropagation();
    }
  },

  /** @private */
  tryConnect_() {
    if (!this.isDisconnected_(this.device)) {
      return;
    }

    this.fire('device-event', {
      action: 'connect',
      device: this.device,
    });
  },

  /** @private */
  onClick_() {
    this.tryConnect_();
  },

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeyDown_(e) {
    if (e.key === 'Enter' || e.key === ' ') {
      this.tryConnect_();
      e.preventDefault();
    }
  },

  /**
   * @param {!Event} event
   * @private
   */
  onMenuButtonTap_(event) {
    const button = /** @type {!HTMLElement} */ (event.target);
    const menu = /** @type {!CrActionMenuElement} */ (this.$.dotsMenu);
    menu.showAt(button);
    event.stopPropagation();
  },

  /** @private */
  onConnectActionTap_() {
    const action = this.isDisconnected_(this.device) ? 'connect' : 'disconnect';
    this.fire('device-event', {
      action: action,
      device: this.device,
    });
    /** @type {!CrActionMenuElement} */ (this.$.dotsMenu).close();
  },

  /** @private */
  onRemoveTap_() {
    this.fire('device-event', {
      action: 'remove',
      device: this.device,
    });
    /** @type {!CrActionMenuElement} */ (this.$.dotsMenu).close();
  },

  /**
   * @param {boolean} connected
   * @return {string} The text to display for the connect/disconnect menu item.
   * @private
   */
  getConnectActionText_(connected) {
    return this.i18n(connected ? 'bluetoothDisconnect' : 'bluetoothConnect');
  },

  /**
   * @param {!chrome.bluetooth.Device} device
   * @return {string} The text to display for |device| in the device list.
   * @private
   */
  getDeviceName_(device) {
    return device.name || device.address;
  },

  /**
   * @param {!chrome.bluetooth.Device} device
   * @return {string} The aria label to use for a |device| that will include the
   * device name and device type if known.
   * @private
   */
  getAriaLabel_(device) {
    // We need to turn the device name, connection status and type into a single
    // localized string.
    // The possible device type enum values can be seen here:
    // https://developer.chrome.com/apps/bluetooth#type-Device.
    // The localization id is computed dynamically to avoid maintaining a
    // mapping from the enum string value to the localization id.
    // if device.type is not defined, we fall back to an unknown device string.
    const deviceName = this.getDeviceName_(device);
    const deviceStatus = this.getConnectionStatusText_(device);

    const a11ydeviceNameAndType = (device.type) ?
        this.i18n('bluetoothDeviceType_' + device.type, deviceName) :
        this.i18n('bluetoothDeviceType_unknown', deviceName);
    return this.i18n(
        'bluetoothDeviceWithConnectionStatus', a11ydeviceNameAndType,
        deviceStatus);
  },

  /**
   * @param {!chrome.bluetooth.Device} device
   * @return {string} The text to display the connection status of |device|.
   * @private
   */
  getConnectionStatusText_(device) {
    if (!this.hasConnectionStatusText_(device)) {
      return '';
    }
    if (device.connecting) {
      return this.i18n('bluetoothConnecting');
    }
    if (!device.connected) {
      return this.i18n('bluetoothNotConnected');
    }
    return device.batteryPercentage !== undefined ?
        this.i18n('bluetoothConnectedWithBattery', device.batteryPercentage) :
        this.i18n('bluetoothConnected');
  },

  /**
   * @param {!chrome.bluetooth.Device} device
   * @return {boolean} True if connection status should be shown as the
   *     secondary text of the |device| in device list.
   * @private
   */
  hasConnectionStatusText_(device) {
    return !!(device.paired || device.connecting);
  },

  /**
   * @param {!chrome.bluetooth.Device} device
   * @return {boolean}
   * @private
   */
  isDisconnected_(device) {
    return !device.connected && !device.connecting;
  },

  /**
   * Returns device type icon's ID corresponding to the given device.
   * To be consistent with the Bluetooth device list in system menu, this
   * mapping needs to be synced with ash::tray::GetBluetoothDeviceIcon().
   *
   * @param {!chrome.bluetooth.Device} device
   * @return {string}
   * @private
   */
  getDeviceIcon_(device) {
    switch (device.type) {
      case 'computer':
        return 'cr:computer';
      case 'phone':
        return 'os-settings:smartphone';
      case 'audio':
      case 'carAudio':
        return 'os-settings:headset';
      case 'video':
        return 'cr:videocam';
      case 'joystick':
      case 'gamepad':
        return 'os-settings:gamepad';
      case 'keyboard':
      case 'keyboardMouseCombo':
        return 'os-settings:keyboard';
      case 'tablet':
        return 'os-settings:tablet';
      case 'mouse':
        return 'os-settings:mouse';
      default:
        return device.connected ? 'os-settings:bluetooth-connected' :
                                  'cr:bluetooth';
    }
  },

  /**
   * @param {?chrome.bluetooth.Device} newValue
   * @param {?chrome.bluetooth.Device} oldValue
   * @private
   */
  onDeviceChanged_(newValue, oldValue) {
    if (!newValue) {
      this.shouldShowManagedIcon_ = false;
      return;
    }

    this.browserProxy_.isDeviceBlockedByPolicy(newValue.address)
        .then((isBlocked) => {
          this.shouldShowManagedIcon_ = isBlocked;
          if (!this.shouldShowManagedIcon_) {
            // Fire an event in case the tooltip was previously showing for this
            // icon and this icon now is hidden.
            this.fireTooltipStateChangeEvent_(/*showTooltip=*/ false);
          }
        });
  },

  /** @private */
  onShowTooltip_: function() {
    this.fireTooltipStateChangeEvent_(/*showTooltip=*/ true);
  },

  /**
   * @param {boolean} showTooltip
   */
  fireTooltipStateChangeEvent_(showTooltip) {
    this.fire('blocked-tooltip-state-change', {
      address: this.device.address,
      show: showTooltip,
      element: showTooltip ? this.$$('#managedIcon') : undefined,
    });
  },
});
