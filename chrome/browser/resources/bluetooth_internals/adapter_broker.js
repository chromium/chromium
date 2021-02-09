// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import './uuid.mojom-lite.js';
import './device.mojom-lite.js';
import './adapter.mojom-lite.js';
import './bluetooth_internals.mojom-lite.js';

import {NativeEventTarget as EventTarget} from 'chrome://resources/js/cr/event_target.m.js';

/**
 * Javascript for AdapterBroker, served from
 *     chrome://bluetooth-internals/.
 */
/** @typedef {bluetooth.mojom.AdapterRemote} */
let AdapterRemote;
/** @typedef {bluetooth.mojom.DeviceRemote} */
let DeviceRemote;
/** @typedef {bluetooth.mojom.DiscoverySessionRemote} */
let DiscoverySessionRemote;

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
 * @implements {bluetooth.mojom.AdapterObserverInterface}
 */
export class AdapterBroker extends EventTarget {
  /** @param {!AdapterRemote} adapter */
  constructor(adapter) {
    super();
    this.adapterObserverReceiver_ =
        new bluetooth.mojom.AdapterObserverReceiver(this);
    this.adapter_ = adapter;
    this.adapter_.addObserver(
        this.adapterObserverReceiver_.$.bindNewPipeAndPassRemote());
  }

  presentChanged(present) {
    this.dispatchEvent(new CustomEvent('adapterchanged', {
      detail: {
        property: AdapterProperty.PRESENT,
        value: present,
      }
    }));
  }

  poweredChanged(powered) {
    this.dispatchEvent(new CustomEvent('adapterchanged', {
      detail: {
        property: AdapterProperty.POWERED,
        value: powered,
      }
    }));
  }

  discoverableChanged(discoverable) {
    this.dispatchEvent(new CustomEvent('adapterchanged', {
      detail: {
        property: AdapterProperty.DISCOVERABLE,
        value: discoverable,
      }
    }));
  }

  discoveringChanged(discovering) {
    this.dispatchEvent(new CustomEvent('adapterchanged', {
      detail: {
        property: AdapterProperty.DISCOVERING,
        value: discovering,
      }
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
   * @return {!Promise<!bluetooth.mojom.Device>}
   */
  connectToDevice(address) {
    return this.adapter_.connectToDevice(address).then(function(response) {
      if (response.result != bluetooth.mojom.ConnectResult.SUCCESS) {
        // TODO(crbug.com/663394): Replace with more descriptive error
        // messages.
        const ConnectResult = bluetooth.mojom.ConnectResult;
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
   * @return {Promise<{devices: Array<!bluetooth.mojom.DeviceInfo>}>}
   */
  getDevices() {
    return this.adapter_.getDevices();
  }

  /**
   * Gets the current state of the Adapter.
   * @return {Promise<{info: bluetooth.mojom.AdapterInfo}>}
   */
  getInfo() {
    return this.adapter_.getInfo();
  }


  /**
   * Requests the adapter to start a new discovery session.
   * @return {!Promise<!bluetooth.mojom.DiscoverySessionRemote>}
   */
  startDiscoverySession() {
    return this.adapter_.startDiscoverySession().then(function(response) {
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
 * @param {!mojom.BluetoothInternalsHandlerRemote=}
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
      mojom.BluetoothInternalsHandler.getRemote();

  // Get an Adapter service.
  return bluetoothInternalsHandler.getAdapter().then(function(response) {
    if (!response.adapter) {
      throw new Error('Bluetooth Not Supported on this platform.');
    }

    adapterBroker = new AdapterBroker(response.adapter);
    return adapterBroker;
  });
}
