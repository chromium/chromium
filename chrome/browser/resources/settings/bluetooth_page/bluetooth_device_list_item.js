// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a bluetooth device in a list.
 */

Polymer({
  is: 'bluetooth-device-list-item',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * The bluetooth device.
     * @type {!chrome.bluetooth.Device}
     */
    device: {
      type: Object,
    },

    /**
     * Uses getDeviceName_ as a label for a11y. It will be added as an
     * attribute on this top-level bluetooth-device-list-item.
     */
    ariaLabel: {
      type: String,
      notify: true,
      reflectToAttribute: true,
      computed: 'getDeviceName_(device)',
    },
  },

  hostAttributes: {role: 'button'},

  /**
   * @param {!Event} event
   * @private
   */
  ignoreEnterKey_: function(event) {
    if (event.key == 'Enter') {
      event.stopPropagation();
    }
  },

  /**
   * @param {!Event} event
   * @private
   */
  onMenuButtonTap_: function(event) {
    const button = /** @type {!HTMLElement} */ (event.target);
    const menu = /** @type {!CrActionMenuElement} */ (this.$.dotsMenu);
    menu.showAt(button);
    event.stopPropagation();
  },

  /** @private */
  onConnectActionTap_: function() {
    const action = this.isDisconnected_(this.device) ? 'connect' : 'disconnect';
    this.fire('device-event', {
      action: action,
      device: this.device,
    });
    /** @type {!CrActionMenuElement} */ (this.$.dotsMenu).close();
  },

  /** @private */
  onRemoveTap_: function() {
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
  getConnectActionText_: function(connected) {
    return this.i18n(connected ? 'bluetoothDisconnect' : 'bluetoothConnect');
  },

  /**
   * @param {!chrome.bluetooth.Device} device
   * @return {string} The text to display for |device| in the device list.
   * @private
   */
  getDeviceName_: function(device) {
    return device.name || device.address;
  },

  /**
   * @param {!chrome.bluetooth.Device} device
   * @return {string} The text to display the connection status of |device|.
   * @private
   */
  getConnectionStatusText_: function(device) {
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
  hasConnectionStatusText_: function(device) {
    return !!(device.paired || device.connecting);
  },

  /**
   * @param {!chrome.bluetooth.Device} device
   * @return {boolean}
   * @private
   */
  isDisconnected_: function(device) {
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
  getDeviceIcon_: function(device) {
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
});
