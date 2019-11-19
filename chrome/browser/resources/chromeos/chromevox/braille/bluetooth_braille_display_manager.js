// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Abstracts away the details of chrome.bluetooth,
 * chrome.bluetoothPrivate, and chrome.brailleDisplayPrivate for a UI component
 * to interact with a bluetooth braille display.
 */

goog.provide('BluetoothBrailleDisplayManager');
goog.provide('BluetoothBrailleDisplayListener');

/**
 * @interface
 */
BluetoothBrailleDisplayListener = function() {};

BluetoothBrailleDisplayListener.prototype = {
  /**
   * @param {!Array<chrome.bluetooth.Device>} displays
   */
  onDisplayListChanged: function(displays) {},
  /**
   * Called when a pincode is requested and a response can be made by calling
   * BluetoothBrailleDisplayManager.finishPairing.
   * @param {!chrome.bluetooth.Device} display
   */
  onPincodeRequested: function(display) {},
};

/**
 * Manages interaction with the bluetooth and braille subsystems in Chrome.
 *
 * A caller can use this class by doing:
 * var manager = new BluetoothBrailleDisplayManager();
 * manager.addListener(listenerObject); // listenerObject receives updates on
 *                                     // important events in bluetooth.
 * manager.start();  // Starts bluetooth discovery.
 * manager.connect(); // Connects to a discovered device received in the
 *                   // listenerObject.
 * manager.finishPairing(); // If a pairing request is sent to the
 *                          // listenerObject, this is how a caller can respond.
 * manager.stop(); // Stops discovery, but persists connections.
 * @constructor
 */
BluetoothBrailleDisplayManager = function() {
  /** @private {!Array<BluetoothBrailleDisplayListener>} */
  this.listeners_ = [];

  /**
   * This list of braille display names was taken from other services that
   * utilize Brltty (e.g. BrailleBack).
   * @private {!Array<string|RegExp>}
   */
  this.displayNames_ = [
    '"EL12-', 'Esys-', 'Focus 14 BT', 'Focus 40 BT', 'Brailliant BI',
    /Hansone|HansoneXL|BrailleSense|BrailleEDGE|SmartBeetle/, 'Refreshabraille',
    'Orbit', 'Baum SuperVario', 'VarioConnect', 'VarioUltra', 'HWG Brailliant',
    'braillex trio', /Alva BC/i, 'TSM', 'TS5',
    new RegExp(
        '(Actilino.*|Active Star.*|Braille Wave( BRW)?|Braillino( BL2)?' +
        '|Braille Star 40( BS4)?|Easy Braille( EBR)?|Active Braille( AB4)?' +
        '|Basic Braille BB[3,4,6]?)\\/[a-zA-Z][0-9]-[0-9]{5}'),
    new RegExp('(BRW|BL2|BS4|EBR|AB4|BB(3|4|6)?)\\/[a-zA-Z][0-9]-[0-9]{5}')
  ];

  /**
   * The display explicitly preferred by a caller via connect. Only one such
   * display exists at a time.
   * @private {string?}
   */
  this.preferredDisplayAddress_ =
      localStorage['preferredBrailleDisplayAddress'];

  /**
   * Tracks whether the preferred display is connected.
   * @private {boolean|null|undefined}
   */
  this.preferredDisplayConnected_;

  chrome.bluetooth.onDeviceAdded.addListener(
      this.handleDevicesChanged.bind(this));
  chrome.bluetooth.onDeviceChanged.addListener(
      this.handleDevicesChanged.bind(this));
  chrome.bluetooth.onDeviceRemoved.addListener(
      this.handleDevicesChanged.bind(this));
  chrome.bluetoothPrivate.onPairing.addListener(this.handlePairing.bind(this));
};

BluetoothBrailleDisplayManager.prototype = {
  /**
   * Adds a new listener.
   * @param {BluetoothBrailleDisplayListener} listener
   */
  addListener: function(listener) {
    this.listeners_.push(listener);
  },

  /**
   * Starts discovering bluetooth devices.
   */
  start: function() {
    chrome.bluetooth.startDiscovery();

    // Pick up any devices already in the system including previously paired,
    // but out of range displays.
    this.handleDevicesChanged();
  },

  /**
   * Stops discovering bluetooth devices.
   */
  stop: function() {
    chrome.bluetooth.stopDiscovery();
  },

  /**
   * Connects to the given display.
   *@param{!chrome.bluetooth.Device} display
   */
  connect: function(display) {
    if (this.preferredDisplayAddress_ === display.address ||
        !this.preferredDisplayAddress_) {
      this.connectInternal(display);
    } else {
      chrome.bluetoothPrivate.disconnectAll(
          this.preferredDisplayAddress_, () => {
            this.connectInternal(display);
          });
    }
  },

  /**
   * @param{!chrome.bluetooth.Device} display
   * @protected *
   */
  connectInternal: function(display) {
    this.preferredDisplayAddress_ = display.address;
    localStorage['preferredBrailleDisplayAddress'] = display.address;
    if (!display.connected) {
      chrome.bluetoothPrivate.connect(display.address, (result) => {
        if (!display.paired) {
          chrome.bluetoothPrivate.pair(display.address);
        }
      });
      return;
    }

    if (!display.paired) {
      chrome.bluetoothPrivate.pair(display.address);
    }
  },

  /**
   * Disconnects the given display and clears it from Brltty.
   * @param{!chrome.bluetooth.Device} display
   */
  disconnect: function(display) {
    chrome.bluetoothPrivate.disconnectAll(display.address);
    chrome.brailleDisplayPrivate.updateBluetoothBrailleDisplayAddress('');
  },

  /**
   * Forgets the given display.
   * @param {!chrome.bluetooth.Device} display
   */
  forget: function(display) {
    chrome.bluetoothPrivate.forgetDevice(display.address);
    chrome.brailleDisplayPrivate.updateBluetoothBrailleDisplayAddress('');
  },

  /**
   *  Finishes pairing in response to
   * BluetoothBrailleDisplayListener.onPincodeRequested.
   * @param{!chrome.bluetooth.Device} display
   * @param{string} pincode *
   */
  finishPairing: function(display, pincode) {
    chrome.bluetoothPrivate.setPairingResponse(
        {response: 'confirm', device: display, pincode: pincode}, () => {});
  },

  /**
   * @param{ chrome.bluetooth.Device=} opt_device
   * @protected
   */
  handleDevicesChanged: function(opt_device) {
    chrome.bluetooth.getDevices((devices) => {
      var displayList = devices.filter((device) => {
        return this.displayNames_.some((name) => {
          return device.name && device.name.search(name) == 0;
        });
      });
      if (displayList.length == 0) {
        return;
      }
      if (opt_device && !displayList.find((i) => i.name == opt_device.name)) {
        return;
      }

      displayList.forEach((display) => {
        if (this.preferredDisplayAddress_ == display.address) {
          this.handlePreferredDisplayConnectionStateChanged(display);
        }
      });
      this.listeners_.forEach((listener) => {
        listener.onDisplayListChanged(displayList);
      });
    });
  },

  /**
   * @param{chrome.bluetoothPrivate.PairingEvent} pairingEvent
   * @protected
   */
  handlePairing: function(pairingEvent) {
    if (pairingEvent.pairing ==
        chrome.bluetoothPrivate.PairingEventType.REQUEST_PINCODE)
      this.listeners_.forEach(
          (listener) => listener.onPincodeRequested(pairingEvent.device));
  },

  /**
   * @param{chrome.bluetooth.Device} display
   * @protected
   */
  handlePreferredDisplayConnectionStateChanged: function(display) {
    if (display.connected === this.preferredDisplayConnected_) {
      return;
    }

    this.preferredDisplayConnected_ = display.connected;

    // We do not clear the address seen by Brltty unless the caller explicitly
    // disconnects or forgets the display via the public methods of this class.
    if (display.connected) {
      chrome.brailleDisplayPrivate.updateBluetoothBrailleDisplayAddress(
          display.address);
    }
  }
};
