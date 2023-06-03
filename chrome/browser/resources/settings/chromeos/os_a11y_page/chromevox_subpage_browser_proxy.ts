// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

export type DeviceEventListener = (device?: chrome.bluetooth.Device) => void;
export type PairingEventListener =
    (pairingEvent: chrome.bluetoothPrivate.PairingEvent) => void;

export interface ChromeVoxSubpageBrowserProxy {
  /**
   * Requests the updated voice data. Returned by the 'all-voice-data-updated'
   * WebUI Listener event.
   */
  getAllTtsVoiceData(): void;

  /**
   * Triggers the TtsPlatform to update its list of voices and relay that update
   * through VoicesChanged.
   */
  refreshTtsVoices(): void;

  /**
   * Gets the display name for `locale` in the system lagnuage.
   */
  getDisplayNameForLocale(locale: string): Promise<string>;

  /**
   * Gets the current application locale.
   */
  getApplicationLocale(): Promise<string>;

  /**
   * Bluetooth API handlers for Bluetooth Braille Display Settings.
   */
  addDeviceAddedListener(listener: DeviceEventListener): void;
  removeDeviceAddedListener(listener: DeviceEventListener): void;
  addDeviceChangedListener(listener: DeviceEventListener): void;
  removeDeviceChangedListener(listener: DeviceEventListener): void;
  addDeviceRemovedListener(listener: DeviceEventListener): void;
  removeDeviceRemovedListener(listener: DeviceEventListener): void;
  addPairingListener(listener: PairingEventListener): void;
  removePairingListener(listener: PairingEventListener): void;
  startDiscovery(): void;
  stopDiscovery(): void;

  /**
   * Changes the currently selected bluetooth braille display.
   */
  updateBluetoothBrailleDisplayAddress(displayAddress: string): void;
}

let instance: ChromeVoxSubpageBrowserProxy|null = null;

export class ChromeVoxSubpageBrowserProxyImpl implements
    ChromeVoxSubpageBrowserProxy {
  static getInstance(): ChromeVoxSubpageBrowserProxy {
    return instance || (instance = new ChromeVoxSubpageBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: ChromeVoxSubpageBrowserProxy): void {
    instance = obj;
  }

  getAllTtsVoiceData(): void {
    chrome.send('getAllTtsVoiceData');
  }

  refreshTtsVoices(): void {
    chrome.send('refreshTtsVoices');
  }

  getDisplayNameForLocale(locale: string): Promise<string> {
    return sendWithPromise('getDisplayNameForLocale', locale);
  }

  getApplicationLocale(): Promise<string> {
    return sendWithPromise('getApplicationLocale');
  }

  addDeviceAddedListener(listener: DeviceEventListener): void {
    chrome.bluetooth.onDeviceAdded.addListener(listener);
  }

  removeDeviceAddedListener(listener: DeviceEventListener): void {
    chrome.bluetooth.onDeviceAdded.removeListener(listener);
  }

  addDeviceChangedListener(listener: DeviceEventListener): void {
    chrome.bluetooth.onDeviceChanged.addListener(listener);
  }

  removeDeviceChangedListener(listener: DeviceEventListener): void {
    chrome.bluetooth.onDeviceChanged.removeListener(listener);
  }

  addDeviceRemovedListener(listener: DeviceEventListener): void {
    chrome.bluetooth.onDeviceRemoved.addListener(listener);
  }

  removeDeviceRemovedListener(listener: DeviceEventListener): void {
    chrome.bluetooth.onDeviceRemoved.removeListener(listener);
  }

  addPairingListener(listener: PairingEventListener): void {
    chrome.bluetoothPrivate.onPairing.addListener(listener);
  }

  removePairingListener(listener: PairingEventListener): void {
    chrome.bluetoothPrivate.onPairing.removeListener(listener);
  }

  startDiscovery(): void {
    chrome.bluetooth.startDiscovery();
  }

  stopDiscovery(): void {
    chrome.bluetooth.startDiscovery();
  }

  updateBluetoothBrailleDisplayAddress(displayAddress: string): void {
    chrome.send('updateBluetoothBrailleDisplayAddress', [displayAddress]);
  }
}
