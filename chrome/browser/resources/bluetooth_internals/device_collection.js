// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for DeviceCollection, served from
 *     chrome://bluetooth-internals/.
 */

import {assert} from 'chrome://resources/js/assert.js';

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
 * Collection of devices. Extends EventTarget to notify observers when the
 * collection changes.
 */
export class DeviceCollection extends EventTarget {
  /**
   * @param {!Array<!DeviceInfo>} array The starting
   *     collection of devices.
   */
  constructor(array) {
    super();

    /** @private {!Array<!DeviceInfo>} */
    this.array_ = array;

    // Keep track of MAC addresses which were previously found via scan, but
    // are no longer being advertised or nearby. Used to inform isRemoved().
    /** @private {!Object<string, boolean>} */
    this.removedDevices_ = {};
  }

  /** @return {number} */
  get length() {
    return this.array_.length;
  }

  /**
   * @param {number}
   * @return {DeviceInfo}
   */
  item(index) {
    return (index < 0 || index > this.length) ? undefined : this.array_[index];
  }

  /**
   * Finds the Device in the collection with the matching address.
   * @param {string} address
   * @return {number} The index where the device was found.
   */
  getByAddress(address) {
    for (let i = 0; i < this.length; i++) {
      const device = this.array_[i];
      if (address === device.address) {
        return i;
      }
    }
    return -1;
  }

  /**
   * Adds or updates a Device with new DeviceInfo.
   * @param {!DeviceInfo} deviceInfo
   */
  addOrUpdate(deviceInfo) {
    this.removedDevices_[deviceInfo.address] = false;
    const oldDeviceIndex = this.getByAddress(deviceInfo.address);

    if (oldDeviceIndex !== -1) {
      // Update rssi if it's valid
      const oldDeviceInfo = this.array_[oldDeviceIndex];
      const rssi = (deviceInfo.rssi && deviceInfo.rssi.value) ||
          (oldDeviceInfo.rssi && oldDeviceInfo.rssi.value);

      // The rssi property may be null, so it must be re-assigned.
      Object.assign(oldDeviceInfo, deviceInfo);
      oldDeviceInfo.rssi = {value: rssi};
      this.dispatchEvent(new CustomEvent(
          'device-update',
          {bubbles: true, composed: true, detail: oldDeviceIndex}));
    } else {
      this.array_.push(deviceInfo);
      this.dispatchEvent(new CustomEvent('device-added', {
        bubbles: true,
        composed: true,
        detail: {index: this.length - 1, device: deviceInfo},
      }));
    }
  }

  /**
   * Marks the Device as removed.
   * @param {!DeviceInfo} deviceInfo
   */
  remove(deviceInfo) {
    const deviceIndex = this.getByAddress(deviceInfo.address);
    assert(deviceIndex !== -1, 'Device does not exist.');
    this.removedDevices_[deviceInfo.address] = true;
    this.dispatchEvent(new CustomEvent(
        'device-update', {bubbles: true, composed: true, detail: deviceIndex}));
  }

  /**
   * Return true if device was "removed" -- previously found via scan but
   * either no longer advertising or no longer nearby.
   * @param {!DeviceInfo} deviceInfo
   * @return {boolean}
   */
  isRemoved(deviceInfo) {
    return !!this.removedDevices_[deviceInfo.address];
  }

  resetForTest() {
    this.array_ = [];
    this.removedDevices_ = [];

    this.dispatchEvent(new CustomEvent(
        'devices-reset-for-test', {bubbles: true, composed: true}));
  }
}
