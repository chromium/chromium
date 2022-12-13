// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/ash/common/assert.js';
import {MultiDeviceSetupInterface} from 'chrome://resources/mojo/chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom-webui.js';

/**
 * @implements {MultiDeviceSetupInterface}
 */
export class FakeMojoService {
  constructor() {
    /**
     * The number of devices to return in a getEligibleHostDevices() call.
     * @type {number}
     */
    this.deviceCount = 2;

    /**
     * Whether calls to setHostDevice() should succeed.
     * @type {boolean}
     */
    this.shouldSetHostSucceed = true;
  }

  /** @override */
  setAccountStatusChangeDelegate(delegate) {
    // Unimplemented; never called from setup flow.
    assertNotReached();
  }

  /** @override */
  addHostStatusObserver(observer) {
    // Unimplemented; never called from setup flow.
    assertNotReached();
  }

  /** @override */
  addFeatureStateObserver(observer) {
    // Unimplemented; never called from setup flow.
    assertNotReached();
  }

  /** @override */
  getEligibleHostDevices() {
    const deviceNames = ['Pixel', 'Pixel XL', 'Nexus 5', 'Nexus 6P'];
    const devices = [];
    for (let i = 0; i < this.deviceCount; i++) {
      const deviceName = deviceNames[i % 4];
      devices.push({deviceName: deviceName, deviceId: deviceName + '--' + i});
    }
    return new Promise(function(resolve, reject) {
      resolve({eligibleHostDevices: devices});
    });
  }

  /** @override */
  getEligibleActiveHostDevices() {
    const deviceNames = ['Pixel', 'Pixel XL', 'Nexus 5', 'Nexus 6P'];
    const devices = [];
    for (let i = 0; i < this.deviceCount; i++) {
      const deviceName = deviceNames[i % 4];
      devices.push({
        remoteDevice: {deviceName: deviceName, deviceId: deviceName + '--' + i},
      });
    }
    return new Promise(function(resolve, reject) {
      resolve({eligibleHostDevices: devices});
    });
  }

  /** @override */
  setHostDevice(hostInstanceIdOrLegacyDeviceId) {
    if (this.shouldSetHostSucceed) {
      console.info(
          'setHostDevice(' + hostInstanceIdOrLegacyDeviceId +
          ') called; simulating ' +
          'success.');
    } else {
      console.warn('setHostDevice() called; simulating failure.');
    }
    return new Promise((resolve, reject) => {
      resolve({success: this.shouldSetHostSucceed});
    });
  }

  /** @override */
  removeHostDevice() {
    // Unimplemented; never called from setup flow.
    assertNotReached();
  }

  /** @override */
  getHostStatus() {
    return new Promise((resolve, reject) => {
      reject('Unimplemented; never called from setup flow.');
    });
  }

  /** @override */
  setFeatureEnabledState() {
    return new Promise((resolve, reject) => {
      reject('Unimplemented; never called from setup flow.');
    });
  }

  /** @override */
  getFeatureStates() {
    return new Promise((resolve, reject) => {
      reject('Unimplemented; never called from setup flow.');
    });
  }

  /** @override */
  retrySetHostNow() {
    return new Promise((resolve, reject) => {
      reject('Unimplemented; never called from setup flow.');
    });
  }

  /** @override */
  triggerEventForDebugging(type) {
    return new Promise((resolve, reject) => {
      reject('Unimplemented; never called from setup flow.');
    });
  }
}
