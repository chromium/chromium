// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Abstracts away the details of chrome.bluetooth,
 * chrome.bluetoothPrivate, and chrome.brailleDisplayPrivate for a UI component
 * to interact with a bluetooth braille display.
 */
import {LocalStorage} from '../../common/local_storage.js';

/** @interface */
export class BluetoothBrailleDisplayListener {
  /**
   * @param {!Array<chrome.bluetooth.Device>} displays
   */
  onDisplayListChanged(displays) {}

  /**
   * Called when a pincode is requested and a response can be made by calling
   * BluetoothBrailleDisplayManager.finishPairing.
   * @param {!chrome.bluetooth.Device} display
   */
  onPincodeRequested(display) {}
}


/**
 * Manages interaction with the bluetooth and braille subsystems in Chrome.
 * A caller can use this class by doing:
 * let manager = new BluetoothBrailleDisplayManager();
 * manager.addListener(listenerObject); // listenerObject receives updates on
 *                                     // important events in bluetooth.
 * manager.start();  // Starts bluetooth discovery.
 * manager.connect(); // Connects to a discovered device received in the
 *                   // listenerObject.
 * manager.finishPairing(); // If a pairing request is sent to the
 *                          // listenerObject, this is how a caller can respond.
 * manager.stop(); // Stops discovery, but persists connections.
 */
export class BluetoothBrailleDisplayManager {
  constructor() {
    /** @private {!Array<BluetoothBrailleDisplayListener>} */
    this.listeners_ = [];

    /**
     * This list of braille display names was taken from other services that
     * utilize Brltty (e.g. BrailleBack).
     * @private {!Array<string|RegExp>}
     */
    this.displayNamePrefixes_ = [
      'Actilino ALO',
      'Activator AC4',
      'Active Braille AB',
      'Active Star AS',
      'ALVA BC',
      'APH Chameleon',
      'APH Mantis',
      'Basic Braille BB',
      'Basic Braille Plus BP',
      'BAUM Conny',
      'Baum PocketVario',
      'Baum SuperVario',
      'Baum SVario',
      'BrailleConnect',
      'BrailleEDGE',
      'BrailleMe',
      'BMpk',
      'BMsmart',
      'BM32',
      'BrailleNote Touch',
      'BrailleSense',
      'Braille Star',
      'Braillex',
      'Brailliant BI',
      'Brailliant 14',
      'Brailliant 80',
      'Braillino BL',
      'B2G',
      'Conny',
      'Easy Braille EBR',
      'EL12-',
      'Esys-',
      'Focus',
      'Humanware BrailleOne',
      'HWG Brailliant',
      'MB248',
      'NLS eReader',
      'Orbit Reader',
      'Pronto!',
      'Refreshabraille',
      'SmartBeetle',
      'SuperVario',
      'TSM',
      'VarioConnect',
      'VarioUltra',
    ];

    /**
     * The display explicitly preferred by a caller via connect. Only one such
     * display exists at a time.
     * @private {string?}
     */
    this.preferredDisplayAddress_ =
        LocalStorage.get('preferredBrailleDisplayAddress');

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
    chrome.bluetoothPrivate.onPairing.addListener(
        this.handlePairing.bind(this));
  }

  /**
   * Adds a new listener.
   * @param {BluetoothBrailleDisplayListener} listener
   */
  addListener(listener) {
    this.listeners_.push(listener);
  }

  /**
   * Starts discovering bluetooth devices.
   */
  start() {
    chrome.bluetooth.startDiscovery();

    // Pick up any devices already in the system including previously paired,
    // but out of range displays.
    this.handleDevicesChanged();
  }

  /**
   * Stops discovering bluetooth devices.
   */
  stop() {
    chrome.bluetooth.stopDiscovery();
  }

  /**
   * Connects to the given display.
   *@param{!chrome.bluetooth.Device} display
   */
  connect(display) {
    if (this.preferredDisplayAddress_ === display.address ||
        !this.preferredDisplayAddress_) {
      this.connectInternal(display);
    } else {
      chrome.bluetoothPrivate.disconnectAll(
          this.preferredDisplayAddress_, () => {
            this.connectInternal(display);
          });
    }
  }

  /**
   * @param{!chrome.bluetooth.Device} display
   * @protected *
   */
  connectInternal(display) {
    this.preferredDisplayAddress_ = display.address;
    LocalStorage.set('preferredBrailleDisplayAddress', display.address);
    if (!display.connected) {
      chrome.bluetoothPrivate.connect(display.address, result => {
        if (!display.paired) {
          chrome.bluetoothPrivate.pair(display.address);
        }
      });
      return;
    }

    if (!display.paired) {
      chrome.bluetoothPrivate.pair(display.address);
    }
  }

  /**
   * Disconnects the given display and clears it from Brltty.
   * @param{!chrome.bluetooth.Device} display
   */
  disconnect(display) {
    chrome.bluetoothPrivate.disconnectAll(display.address);
    chrome.brailleDisplayPrivate.updateBluetoothBrailleDisplayAddress('');
  }

  /**
   * Forgets the given display.
   * @param {!chrome.bluetooth.Device} display
   */
  forget(display) {
    chrome.bluetoothPrivate.forgetDevice(display.address);
    chrome.brailleDisplayPrivate.updateBluetoothBrailleDisplayAddress('');
  }

  /**
   *  Finishes pairing in response to
   * BluetoothBrailleDisplayListener.onPincodeRequested.
   * @param{!chrome.bluetooth.Device} display
   * @param{string} pincode *
   */
  finishPairing(display, pincode) {
    chrome.bluetoothPrivate.setPairingResponse(
        {response: 'confirm', device: display, pincode}, () => {});
  }

  /**
   * @param{ chrome.bluetooth.Device=} opt_device
   * @protected
   */
  handleDevicesChanged(opt_device) {
    chrome.bluetooth.getDevices(devices => {
      const displayList = devices.filter(device => {
        return this.displayNamePrefixes_.some(name => {
          return device.name && device.name.search(name) === 0;
        });
      });
      if (displayList.length === 0) {
        return;
      }
      if (opt_device && !displayList.find(i => i.name === opt_device.name)) {
        return;
      }

      displayList.forEach(display => {
        if (this.preferredDisplayAddress_ === display.address) {
          this.handlePreferredDisplayConnectionStateChanged(display);
        }
      });
      this.listeners_.forEach(
          listener => listener.onDisplayListChanged(displayList));
    });
  }

  /**
   * @param{chrome.bluetoothPrivate.PairingEvent} pairingEvent
   * @protected
   */
  handlePairing(pairingEvent) {
    if (pairingEvent.pairing ===
        chrome.bluetoothPrivate.PairingEventType.REQUEST_PINCODE) {
      this.listeners_.forEach(
          listener => listener.onPincodeRequested(pairingEvent.device));
    }
  }

  /**
   * @param{chrome.bluetooth.Device} display
   * @protected
   */
  handlePreferredDisplayConnectionStateChanged(display) {
    if (display.connected === this.preferredDisplayConnected_) {
      return;
    }

    this.preferredDisplayConnected_ = display.connected;

    // We do not clear the address seen by Brltty unless the caller explicitly
    // disconnects or forgets the display via the public methods of this
    // class.
    if (display.connected) {
      chrome.brailleDisplayPrivate.updateBluetoothBrailleDisplayAddress(
          display.address);
    }
  }
}
