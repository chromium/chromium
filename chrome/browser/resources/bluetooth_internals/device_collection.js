// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for DeviceCollection, served from
 *     chrome://bluetooth-internals/.
 */

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import './device.mojom-lite.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {ArrayDataModel} from 'chrome://resources/js/cr/ui/array_data_model.m.js';

/**
 * Enum of connection status for a device.
 * @enum {number}
 */
export const ConnectionStatus = {
  DISCONNECTED: 0,
  CONNECTING: 1,
  CONNECTED: 2,
};

/**
 * Collection of devices. Extends ArrayDataModel which provides a set of
 * functions and events that notifies observers when the collection changes.
 */
export class DeviceCollection extends ArrayDataModel {
  /**
   * @param {!Array<!bluetooth.mojom.DeviceInfo>} array The starting
   *     collection of devices.
   */
  constructor(array) {
    super(array);

    // Keep track of MAC addresses which were previously found via scan, but
    // are no longer being advertised or nearby. Used to inform isRemoved().
    /** @private {!Object<string, boolean>} */
    this.removedDevices_ = {};
  }

  /**
   * Finds the Device in the collection with the matching address.
   * @param {string} address
   */
  getByAddress(address) {
    for (let i = 0; i < this.length; i++) {
      const device = this.item(i);
      if (address == device.address) {
        return device;
      }
    }
    return null;
  }

  /**
   * Adds or updates a Device with new DeviceInfo.
   * @param {!bluetooth.mojom.DeviceInfo} deviceInfo
   */
  addOrUpdate(deviceInfo) {
    this.removedDevices_[deviceInfo.address] = false;
    const oldDeviceInfo = this.getByAddress(deviceInfo.address);

    if (oldDeviceInfo) {
      // Update rssi if it's valid
      const rssi = (deviceInfo.rssi && deviceInfo.rssi.value) ||
          (oldDeviceInfo.rssi && oldDeviceInfo.rssi.value);

      // The rssi property may be null, so it must be re-assigned.
      Object.assign(oldDeviceInfo, deviceInfo);
      oldDeviceInfo.rssi = {value: rssi};
      this.updateIndex(this.indexOf(oldDeviceInfo));
    } else {
      this.push(deviceInfo);
    }
  }

  /**
   * Marks the Device as removed.
   * @param {!bluetooth.mojom.DeviceInfo} deviceInfo
   */
  remove(deviceInfo) {
    const device = this.getByAddress(deviceInfo.address);
    assert(device, 'Device does not exist.');
    this.removedDevices_[deviceInfo.address] = true;
    this.updateIndex(this.indexOf(device));
  }

  /**
   * Return true if device was "removed" -- previously found via scan but
   * either no longer advertising or no longer nearby.
   * @param {!bluetooth.mojom.DeviceInfo} deviceInfo
   */
  isRemoved(deviceInfo) {
    return !!this.removedDevices_[deviceInfo.address];
  }
}
