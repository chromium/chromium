// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AdapterObserverReceiver, AdapterRemote, ConnectResult, DiscoverySessionRemote} from './adapter.mojom-webui.js';
import {BluetoothInternalsHandler, BluetoothInternalsHandlerRemote} from './bluetooth_internals.mojom-webui.js';
import {Device, DeviceRemote} from './device.mojom-webui.js';

const SCAN_CLIENT_NAME = 'Bluetooth Internals Page';

/**
 * Javascript for AdapterBroker, served from
 *     chrome://bluetooth-internals/.
 */

/**
 * Enum of adapter property names. Used for adapterchanged events.
 * @enum {string}
 */
export const AdapterProperty = {
  DISCOVERABLE: 'discoverable',
  DISCOVERING: 'discovering',
  POWERED: 'powered',
  PRESENT: 'present',
};

/**
 * The proxy class of an adapter and router of adapter events.
 * Exposes an EventTarget interface that allows other object to subscribe to
 * to specific AdapterObserver events.
 * Provides remote access to Adapter functions. Converts parameters to Mojo
 * handles and back when necessary.
 *
 * @implements {AdapterObserverInterface}
 */
export class AdapterBroker extends EventTarget {
  /** @param {!AdapterRemote} adapter */
  constructor(adapter) {
    super();
    this.adapterObserverReceiver_ = new AdapterObserverReceiver(this);
    this.adapter_ = adapter;
    this.adapter_.addObserver(
        this.adapterObserverReceiver_.$.bindNewPipeAndPassRemote());
  }

  presentChanged(present) {
    this.dispatchEvent(new CustomEvent('adapterchanged', {
      detail: {
        property: AdapterProperty.PRESENT,
        value: present,
      },
    }));
  }

  poweredChanged(powered) {
    this.dispatchEvent(new CustomEvent('adapterchanged', {
      detail: {
        property: AdapterProperty.POWERED,
        value: powered,
      },
    }));
  }

  discoverableChanged(discoverable) {
    this.dispatchEvent(new CustomEvent('adapterchanged', {
      detail: {
        property: AdapterProperty.DISCOVERABLE,
        value: discoverable,
      },
    }));
  }

  discoveringChanged(discovering) {
    this.dispatchEvent(new CustomEvent('adapterchanged', {
      detail: {
        property: AdapterProperty.DISCOVERING,
        value: discovering,
      },
    }));
  }

  deviceAdded(device) {
    this.dispatchEvent(
        new CustomEvent('deviceadded', {detail: {deviceInfo: device}}));
  }

  deviceChanged(device) {
    this.dispatchEvent(
        new CustomEvent('devicechanged', {detail: {deviceInfo: device}}));
  }

  deviceRemoved(device) {
    this.dispatchEvent(
        new CustomEvent('deviceremoved', {detail: {deviceInfo: device}}));
  }

  /**
   * Creates a GATT connection to the device with |address|.
   * @param {string} address
   * @return {!Promise<!Device>}
   */
  connectToDevice(address) {
    return this.adapter_.connectToDevice(address).then(function(response) {
      if (response.result !== ConnectResult.SUCCESS) {
        // TODO(crbug.com/40492643): Replace with more descriptive error
        // messages.
        const errorString = Object.keys(ConnectResult).find(function(key) {
          return ConnectResult[key] === response.result;
        });

        throw new Error(errorString);
      }

      return response.device;
    });
  }

  /**
   * Gets an array of currently detectable devices from the Adapter service.
   * @return {Promise<{devices: Array<!DeviceInfo>}>}
   */
  getDevices() {
    return this.adapter_.getDevices();
  }

  /**
   * Gets the current state of the Adapter.
   * @return {Promise<{info: AdapterInfo}>}
   */
  getInfo() {
    return this.adapter_.getInfo();
  }


  /**
   * Requests the adapter to start a new discovery session.
   * @return {!Promise<!DiscoverySessionRemote>}
   */
  startDiscoverySession() {
    return this.adapter_.startDiscoverySession(SCAN_CLIENT_NAME)
        .then(function(response) {
          if (!response.session) {
            throw new Error('Discovery session failed to start');
          }

          return response.session;
        });
  }
}

let adapterBroker = null;

/**
 * Initializes an AdapterBroker if one doesn't exist.
 * @param {!BluetoothInternalsHandlerRemote=}
 *     opt_bluetoothInternalsHandler
 * @return {!Promise<!AdapterBroker>} resolves with
 *     AdapterBroker, rejects if Bluetooth is not supported.
 */
export function getAdapterBroker(opt_bluetoothInternalsHandler) {
  if (adapterBroker) {
    return Promise.resolve(adapterBroker);
  }

  const bluetoothInternalsHandler = opt_bluetoothInternalsHandler ?
      opt_bluetoothInternalsHandler :
      BluetoothInternalsHandler.getRemote();

  // Get an Adapter service.
  return bluetoothInternalsHandler.getAdapter().then(function(response) {
    if (!response.adapter) {
      throw new Error('Bluetooth Not Supported on this platform.');
    }

    adapterBroker = new AdapterBroker(response.adapter);
    return adapterBroker;
  });
}
