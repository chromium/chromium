// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for device_broker, served from chrome://bluetooth-internals/.
 * Provides a single source to access DeviceRemotes. DeviceRemotes are cached
 * for repeated use. Multiple connection requests will result in the same
 * DeviceRemote being shared among all requesters.
 */

import {getAdapterBroker} from './adapter_broker.js';
import type {DeviceRemote} from './device.mojom-webui.js';

// Expose for testing.
export const connectedDevices: Map<string, DeviceRemote|Promise<DeviceRemote>> =
    new Map();

/**
 * Creates a GATT connection to the device with |address|. If a connection to
 * the device already exists, the promise is resolved with the existing
 * DeviceRemote. If a connection is in progress, the promise resolves when
 * the existing connection request promise is fulfilled.
 */
export function connectToDevice(address: string): Promise<DeviceRemote> {
  const deviceOrPromise = connectedDevices.get(address) || null;
  if (deviceOrPromise !== null) {
    return Promise.resolve(deviceOrPromise);
  }

  const promise = getAdapterBroker()
                      .then(function(adapterBroker) {
                        return adapterBroker.connectToDevice(address);
                      })
                      .then(function(device) {
                        connectedDevices.set(address, device);

                        device.onConnectionError.addListener(
                            () => connectedDevices.delete(address));

                        return device;
                      })
                      .catch(function(error) {
                        connectedDevices.delete(address);
                        throw error;
                      });

  connectedDevices.set(address, promise);
  return promise;
}
