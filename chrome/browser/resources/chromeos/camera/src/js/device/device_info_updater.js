// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for device.
 */
cca.device = cca.device || {};

/**
 * Contains information of all cameras on the device and will updates its value
 * when any plugin/unplug external camera changes.
 */
cca.device.DeviceInfoUpdater = class {
  /**
   * @param {!cca.device.PhotoConstraintsPreferrer} photoPreferrer
   * @param {!cca.device.VideoConstraintsPreferrer} videoPreferrer
   * @public
   * */
  constructor(photoPreferrer, videoPreferrer) {
    /**
     * @type {!cca.device.PhotoConstraintsPreferrer}
     * @private
     */
    this.photoPreferrer_ = photoPreferrer;

    /**
     * @type {!cca.device.VideoConstraintsPreferrer}
     * @private
     */
    this.videoPreferrer_ = videoPreferrer;

    /**
     * Listeners to be called after new camera information is available.
     * @type {!Array<!function(!cca.device.DeviceInfoUpdater): Promise>}
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
     * @type {!Promise<!Array<!MediaDeviceInfo>>}
     * @private
     */
    this.devicesInfo_ = this.enumerateDevices_();

    /**
     * Camera3DeviceInfo of all available video devices. Is null on HALv1 device
     * without mojo api support.
     * @type {!Promise<?Array<cca.device.Camera3DeviceInfo>>}
     * @private
     */
    this.camera3DevicesInfo_ = this.queryMojoDevicesInfo_();

    /**
     * Promise of first update.
     * @type {!Promise}
     */
    this.firstUpdate_ = this.update_();

    navigator.mediaDevices.addEventListener(
        'devicechange', this.update_.bind(this));
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
    this.devicesInfo_ = this.enumerateDevices_();
    this.camera3DevicesInfo_ = this.queryMojoDevicesInfo_();
    try {
      await this.devicesInfo_;
      const devices = await this.camera3DevicesInfo_;
      if (devices) {
        this.photoPreferrer_.updateDevicesInfo(devices);
        this.videoPreferrer_.updateDevicesInfo(devices);
      }
      await Promise.all(this.deviceChangeListeners_.map((l) => l(this)));
    } catch (e) {
      console.error(e);
    }
  }

  /**
   * Enumerates all available devices and gets their MediaDeviceInfo.
   * @return {!Promise<!Array<!MediaDeviceInfo>>}
   * @throws {Error}
   * @private
   */
  async enumerateDevices_() {
    const devices = (await navigator.mediaDevices.enumerateDevices())
                        .filter((device) => device.kind === 'videoinput');
    if (devices.length === 0) {
      throw new Error('Device list empty.');
    }
    return devices;
  }

  /**
   * Queries Camera3DeviceInfo of available devices through private mojo API.
   * @return {!Promise<?Array<!cca.device.Camera3DeviceInfo>>} Camera3DeviceInfo
   *     of available devices. Maybe null on HALv1 devices without supporting
   *     private mojo api.
   * @throws {Error} Thrown when camera unplugging happens between enumerating
   *     devices and querying mojo APIs with current device info results.
   * @private
   */
  async queryMojoDevicesInfo_() {
    if (!await cca.mojo.DeviceOperator.isSupported()) {
      return null;
    }
    const deviceInfos = await this.devicesInfo_;
    return Promise.all(
        deviceInfos.map((d) => cca.device.Camera3DeviceInfo.create(d)));
  }

  /**
   * Registers listener to be called when state of available devices changes.
   * @param {!function(!cca.device.DeviceInfoUpdater)} listener
   */
  addDeviceChangeListener(listener) {
    this.deviceChangeListeners_.push(listener);
  }

  /**
   * Requests to lock update of device information. This function is preserved
   * for device information reader to lock the update capability so as to ensure
   * getting consistent data between all information providers.
   * @param {!function(!cca.device.DeviceInfoUpdater): Promise} callback Called
   *     after update capability is locked. Getting information from all
   *     providers in callback are guaranteed to be consistent.
   */
  async lockDeviceInfo(callback) {
    await this.firstUpdate_;
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
        await callback(this);
      } finally {
        this.lockingUpdate_ = null;
      }
    })();
    await this.lockingUpdate_;
  }

  /**
   * Gets MediaDeviceInfo for all available video devices.
   * @return {!Promise<!Array<!MediaDeviceInfo>>}
   * @private
   */
  async getDevicesInfo() {
    return this.devicesInfo_;
  }

  /**
   * Gets MediaDeviceInfo of specific video device.
   * @param {string} deviceId Device id of video device to get information from.
   * @return {!Promise<?MediaDeviceInfo>}
   * @private
   */
  async getDeviceInfo(deviceId) {
    const /** !Array<!MediaDeviceInfo> */ infos = await this.getDevicesInfo();
    return infos.find((d) => d.deviceId === deviceId) || null;
  }

  /**
   * Gets Camera3DeviceInfo for all available video devices.
   * @return {!Promise<?Array<!cca.device.Camera3DeviceInfo>>}
   */
  async getCamera3DevicesInfo() {
    return this.camera3DevicesInfo_;
  }

  /**
   * Gets supported photo and video resolutions for specified video device.
   * @param {string} deviceId Device id of the video device.
   * @return {!Promise<!{photo: !ResolutionList, video: !ResolutionList}>}
   *     Supported photo and video resolutions.
   * @throws {Error} May fail on HALv1 device without capability of querying
   *     supported resolutions.
   */
  async getDeviceResolutions(deviceId) {
    const devices = await this.getCamera3DevicesInfo();
    if (!devices) {
      throw new cca.device.LegacyVCDError();
    }
    const info = devices.find((info) => info.deviceId === deviceId);
    return {photo: info.photoResols, video: info.videoResols};
  }
};
