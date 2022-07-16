// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DeviceOperator} from '../mojo/device_operator.js';
// eslint-disable-next-line no-unused-vars
import {Camera3DeviceInfo} from './camera3_device_info.js';
import {
  PhotoConstraintsPreferrer,  // eslint-disable-line no-unused-vars
  VideoConstraintsPreferrer,  // eslint-disable-line no-unused-vars
} from './constraints_preferrer.js';
import {
  DeviceInfo,  // eslint-disable-line no-unused-vars
  StreamManager,
} from './stream_manager.js';

/**
 * Thrown for no camera available on the device.
 */
export class NoCameraError extends Error {
  /**
   * @param {string=} message
   * @public
   */
  constructor(message = 'No camera available on the device') {
    super(message);
    this.name = this.constructor.name;
  }
}

/**
 * Contains information of all cameras on the device and will updates its value
 * when any plugin/unplug external camera changes.
 */
export class DeviceInfoUpdater {
  /**
   * @param {!PhotoConstraintsPreferrer} photoPreferrer
   * @param {!VideoConstraintsPreferrer} videoPreferrer
   * @public
   * */
  constructor(photoPreferrer, videoPreferrer) {
    /**
     * @type {!PhotoConstraintsPreferrer}
     * @private
     */
    this.photoPreferrer_ = photoPreferrer;

    /**
     * @type {!VideoConstraintsPreferrer}
     * @private
     */
    this.videoPreferrer_ = videoPreferrer;

    /**
     * Listeners to be called after new camera information is available.
     * @type {!Array<function(!DeviceInfoUpdater): void>}
     * @private
     */
    this.deviceChangeListeners_ = [];

    /**
     * Action locking update of camera information.
     * @type {?Promise}
     * @private
     */
    this.lockingUpdate_ = null;

    /**
     * Pending camera information update while update capability is locked.
     * @type {?Promise}
     * @private
     */
    this.pendingUpdate_ = null;

    /**
     * MediaDeviceInfo of all available video devices.
     * @type {!Array<!MediaDeviceInfo>}
     * @private
     */
    this.devicesInfo_ = [];

    /**
     * Camera3DeviceInfo of all available video devices. Is null on fake cameras
     * which do not have private mojo API support.
     * @type {?Array<!Camera3DeviceInfo>}
     * @private
     */
    this.camera3DevicesInfo_ = null;

    /**
     * Pending device Information.
     * @type {!Array<!DeviceInfo>}
     * @private
     */
    this.pendingDevicesInfo_ = [];

    StreamManager.getInstance().addRealDeviceChangeListener(
        async (devicesInfo) => {
          this.pendingDevicesInfo_ = devicesInfo;
          await this.update_();
        });
  }

  /**
   * Tries to gain lock and initiates update process.
   * @private
   */
  async update_() {
    if (this.lockingUpdate_) {
      if (this.pendingUpdate_) {
        return;
      }
      this.pendingUpdate_ = (async () => {
        while (this.lockingUpdate_) {
          try {
            await this.lockingUpdate_;
          } catch (e) {
            // Ignore exception from waiting for existing update.
          }
        }
        this.lockingUpdate_ = this.pendingUpdate_;
        this.pendingUpdate_ = null;
        await this.doUpdate_();
        this.lockingUpdate_ = null;
      })();
    } else {
      this.lockingUpdate_ = (async () => {
        await this.doUpdate_();
        this.lockingUpdate_ = null;
      })();
    }
  }

  /**
   * Updates devices information.
   * @private
   */
  async doUpdate_() {
    this.devicesInfo_ = this.pendingDevicesInfo_.map((d) => d.v1Info);
    this.camera3DevicesInfo_ = this.pendingDevicesInfo_.map((d) => d.v3Info);
    // Update preferer if device supports HALv3.
    if (await DeviceOperator.isSupported()) {
      this.photoPreferrer_.updateDevicesInfo(this.camera3DevicesInfo_);
      this.videoPreferrer_.updateDevicesInfo(this.camera3DevicesInfo_);
      this.deviceChangeListeners_.forEach((l) => l(this));
    } else {
      this.camera3DevicesInfo_ = null;
    }
  }

  /**
   * Registers listener to be called when state of available devices changes.
   * @param {function(!DeviceInfoUpdater): void} listener
   */
  addDeviceChangeListener(listener) {
    this.deviceChangeListeners_.push(listener);
  }

  /**
   * Requests to lock update of device information. This function is preserved
   * for device information reader to lock the update capability so as to ensure
   * getting consistent data between all information providers.
   * @param {function(): !Promise} callback Called after
   *     update capability is locked. Getting information from all providers in
   *     callback are guaranteed to be consistent.
   */
  async lockDeviceInfo(callback) {
    await StreamManager.getInstance().deviceUpdate();
    while (this.lockingUpdate_ || this.pendingUpdate_) {
      try {
        await this.lockingUpdate_;
        await this.pendingUpdate_;
      } catch (e) {
        // Ignore exception from waiting for existing update.
      }
    }
    this.lockingUpdate_ = (async () => {
      try {
        await callback();
      } finally {
        this.lockingUpdate_ = null;
      }
    })();
    await this.lockingUpdate_;
  }

  /**
   * Gets MediaDeviceInfo for all available video devices.
   * @return {!Array<!MediaDeviceInfo>}
   */
  getDevicesInfo() {
    return this.devicesInfo_;
  }

  /**
   * Gets MediaDeviceInfo of specific video device.
   * @param {string} deviceId Device id of video device to get information from.
   * @return {?MediaDeviceInfo}
   */
  getDeviceInfo(deviceId) {
    const /** !Array<!MediaDeviceInfo> */ infos = this.getDevicesInfo();
    return infos.find((d) => d.deviceId === deviceId) || null;
  }

  /**
   * Gets Camera3DeviceInfo for all available video devices.
   * @return {?Array<!Camera3DeviceInfo>}
   */
  getCamera3DevicesInfo() {
    return this.camera3DevicesInfo_;
  }
}
