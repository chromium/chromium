// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AdapterObserverReceiver, ConnectResult} from './adapter.mojom-webui.js';
import type {AdapterInfo, AdapterObserverInterface, AdapterRemote, DiscoverySessionRemote} from './adapter.mojom-webui.js';
import {BluetoothInternalsHandler} from './bluetooth_internals.mojom-webui.js';
import type {BluetoothInternalsHandlerRemote} from './bluetooth_internals.mojom-webui.js';
import type {DeviceInfo, DeviceRemote} from './device.mojom-webui.js';

const SCAN_CLIENT_NAME = 'Bluetooth Internals Page';

/**
 * Javascript for AdapterBroker, served from
 *     chrome://bluetooth-internals/.
 */

/**
 * Enum of adapter property names. Used for adapterchanged events.
 */
export enum AdapterProperty {
  DISCOVERABLE = 'discoverable',
  DISCOVERING = 'discovering',
  POWERED = 'powered',
  PRESENT = 'present',
}

/**
 * The proxy class of an adapter and router of adapter events.
 * Exposes an EventTarget interface that allows other object to subscribe to
 * to specific AdapterObserver events.
 * Provides remote access to Adapter functions. Converts parameters to Mojo
 * handles and back when necessary.
 */
export class AdapterBroker extends EventTarget implements
    AdapterObserverInterface {
  private adapterObserverReceiver_: AdapterObserverReceiver;
  private adapter_: AdapterRemote;

  constructor(adapter: AdapterRemote) {
    super();
    this.adapterObserverReceiver_ = new AdapterObserverReceiver(this);
    this.adapter_ = adapter;
    this.adapter_.addObserver(
        this.adapterObserverReceiver_.$.bindNewPipeAndPassRemote());
  }

  presentChanged(present: boolean) {
    this.dispatchEvent(new CustomEvent('adapterchanged', {
      detail: {
        property: AdapterProperty.PRESENT,
        value: present,
      },
    }));
  }

  poweredChanged(powered: boolean) {
    this.dispatchEvent(new CustomEvent('adapterchanged', {
      detail: {
        property: AdapterProperty.POWERED,
        value: powered,
      },
    }));
  }

  discoverableChanged(discoverable: boolean) {
    this.dispatchEvent(new CustomEvent('adapterchanged', {
      detail: {
        property: AdapterProperty.DISCOVERABLE,
        value: discoverable,
      },
    }));
  }

  discoveringChanged(discovering: boolean) {
    this.dispatchEvent(new CustomEvent('adapterchanged', {
      detail: {
        property: AdapterProperty.DISCOVERING,
        value: discovering,
      },
    }));
  }

  deviceAdded(device: DeviceInfo) {
    this.dispatchEvent(
        new CustomEvent('deviceadded', {detail: {deviceInfo: device}}));
  }

  deviceChanged(device: DeviceInfo) {
    this.dispatchEvent(
        new CustomEvent('devicechanged', {detail: {deviceInfo: device}}));
  }

  deviceRemoved(device: DeviceInfo) {
    this.dispatchEvent(
        new CustomEvent('deviceremoved', {detail: {deviceInfo: device}}));
  }

  /**
   * Creates a GATT connection to the device with |address|.
   */
  connectToDevice(address: string): Promise<DeviceRemote> {
    return this.adapter_.connectToDevice(address).then(function(response) {
      if (response.result !== ConnectResult.SUCCESS) {
        // TODO(crbug.com/40492643): Replace with more descriptive error
        // messages.
        throw new Error(ConnectResult[response.result]);
      }

      return response.device!;
    });
  }

  /**
   * Gets an array of currently detectable devices from the Adapter service.
   */
  getDevices(): Promise<{devices: DeviceInfo[]}> {
    return this.adapter_.getDevices();
  }

  /**
   * Gets the current state of the Adapter.
   */
  getInfo(): Promise<{info: AdapterInfo}> {
    return this.adapter_.getInfo();
  }


  /**
   * Requests the adapter to start a new discovery session.
   */
  startDiscoverySession(): Promise<DiscoverySessionRemote> {
    return this.adapter_.startDiscoverySession(SCAN_CLIENT_NAME)
        .then(function(response) {
          if (!response.session) {
            throw new Error('Discovery session failed to start');
          }

          return response.session;
        });
  }
}

let adapterBroker: AdapterBroker|null = null;

/**
 * Initializes an AdapterBroker if one doesn't exist.
 * @return resolves with AdapterBroker, rejects if Bluetooth is not supported.
 */
export function getAdapterBroker(
    bluetoothInternalsHandler?: BluetoothInternalsHandlerRemote):
    Promise<AdapterBroker> {
  if (adapterBroker) {
    return Promise.resolve(adapterBroker);
  }

  const handler = bluetoothInternalsHandler ?
      bluetoothInternalsHandler :
      BluetoothInternalsHandler.getRemote();

  // Get an Adapter service.
  return handler.getAdapter().then(function(response) {
    if (!response.adapter) {
      throw new Error('Bluetooth Not Supported on this platform.');
    }

    adapterBroker = new AdapterBroker(response.adapter);
    return adapterBroker;
  });
}
