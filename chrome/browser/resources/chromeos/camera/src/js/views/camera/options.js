// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for views.
 */
cca.views = cca.views || {};

/**
 * Namespace for Camera view.
 */
cca.views.camera = cca.views.camera || {};

/**
 * Creates a controller for the options of Camera view.
 * @param {cca.device.DeviceInfoUpdater} infoUpdater
 * @param {function()} doSwitchDevice Callback to trigger device switching.
 * @constructor
 */
cca.views.camera.Options = function(infoUpdater, doSwitchDevice) {
  /**
   * @type {cca.device.DeviceInfoUpdater}
   * @private
   */
  this.infoUpdater_ = infoUpdater;

  /**
   * @type {function()}
   * @private
   */
  this.doSwitchDevice_ = doSwitchDevice;

  /**
   * @type {HTMLInputElement}
   * @private
   */
  this.toggleMic_ = document.querySelector('#toggle-mic');

  /**
   * @type {HTMLInputElement}
   * @private
   */
  this.toggleMirror_ = document.querySelector('#toggle-mirror');

  /**
   * Device id of the camera device currently used or selected.
   * @type {?string}
   * @private
   */
  this.videoDeviceId_ = null;

  /**
   * Whether list of video devices is being refreshed now.
   * @type {boolean}
   * @private
   */
  this.refreshingVideoDeviceIds_ = false;

  /**
   * List of available video devices.
   * @type {Promise<!Array<MediaDeviceInfo>>}
   * @private
   */
  this.videoDevices_ = null;

  /**
   * Promise for querying Camera3DeviceInfo of all available video devices from
   * mojo private API.
   * @type {Promise<!Array<cca.device.Camera3DeviceInfo>>}
   * @private
   */
  this.devicesPrivateInfo_ = null;

  /**
   * Whether the current device is HALv1 and lacks facing configuration.
   * get facing information.
   * @type {?boolean}
   * private
   */
  this.isV1NoFacingConfig_ = null;

  /**
   * Mirroring set per device.
   * @type {Object}
   * @private
   */
  this.mirroringToggles_ = {};

  /**
   * Current audio track in use.
   * @type {MediaStreamTrack}
   * @private
   */
  this.audioTrack_ = null;

  // End of properties, seal the object.
  Object.seal(this);

  [['#switch-device', () => this.switchDevice_()],
   ['#toggle-grid', () => this.animatePreviewGrid_()],
   ['#open-settings', () => cca.nav.open('settings')],
  ]
      .forEach(
          ([selector, fn]) =>
              document.querySelector(selector).addEventListener('click', fn));

  this.toggleMic_.addEventListener('click', () => this.updateAudioByMic_());
  this.toggleMirror_.addEventListener('click', () => this.saveMirroring_());

  // Restore saved mirroring states per video device.
  cca.proxy.browserProxy.localStorageGet(
      {mirroringToggles: {}},
      (values) => this.mirroringToggles_ = values.mirroringToggles);
  // Remove the deprecated values.
  cca.proxy.browserProxy.localStorageRemove(
      ['effectIndex', 'toggleMulti', 'toggleMirror']);

  this.infoUpdater_.addDeviceChangeListener(async (updater) => {
    cca.state.set('multi-camera', (await updater.getDevicesInfo()).length >= 2);
  });
};

cca.views.camera.Options.prototype = {
  /**
   * Device id of the camera device currently used or selected.
   * @return {string}
   */
  get currentDeviceId() {
    return this.videoDeviceId_;
  },
};

/**
 * Switches to the next available camera device.
 * @private
 */
cca.views.camera.Options.prototype.switchDevice_ = async function() {
  if (!cca.state.get('streaming') || cca.state.get('taking')) {
    return;
  }
  const devices = await this.infoUpdater_.getDevicesInfo();
  cca.util.animateOnce(document.querySelector('#switch-device'));
  var index =
      devices.findIndex((entry) => entry.deviceId === this.videoDeviceId_);
  if (index === -1) {
    index = 0;
  }
  if (devices.length > 0) {
    index = (index + 1) % devices.length;
    this.videoDeviceId_ = devices[index].deviceId;
  }
  await this.doSwitchDevice_();
};

/**
 * Animates the preview grid.
 * @private
 */
cca.views.camera.Options.prototype.animatePreviewGrid_ = function() {
  Array.from(document.querySelector('#preview-grid').children).forEach(
      (grid) => cca.util.animateOnce(grid));
};

/**
 * Updates the options' values for the current constraints and stream.
 * @param {MediaStream} stream Current Stream in use.
 * @return {?string} Facing-mode in use.
 */
cca.views.camera.Options.prototype.updateValues = async function(stream) {
  var track = stream.getVideoTracks()[0];
  var trackSettings = track.getSettings && track.getSettings();
  let facingMode = trackSettings && trackSettings.facingMode;
  if (this.isV1NoFacingConfig_ === null) {
    // Because the facing mode of external camera will be set to undefined on
    // all devices, to distinguish HALv1 device without facing configuration,
    // assume the first opened camera is built-in camera. Device without facing
    // configuration won't set facing of built-in cameras. Also if HALv1 device
    // with facing configuration opened external camera first after CCA launched
    // the logic here may misjudge it as this category.
    this.isV1NoFacingConfig_ = facingMode === undefined;
  }
  facingMode = this.isV1NoFacingConfig_ ? null : facingMode || 'external';
  this.videoDeviceId_ = trackSettings && trackSettings.deviceId || null;
  this.updateMirroring_(facingMode);
  this.audioTrack_ = stream.getAudioTracks()[0];
  this.updateAudioByMic_();
  return facingMode;
};

/**
 * Updates mirroring for a new stream.
 * @param {string} facingMode Facing-mode of the stream.
 * @private
 */
cca.views.camera.Options.prototype.updateMirroring_ = function(facingMode) {
  // Update mirroring by detected facing-mode. Enable mirroring by default if
  // facing-mode isn't available.
  var enabled = facingMode ? facingMode !== 'environment' : true;

  // Override mirroring only if mirroring was toggled manually.
  if (this.videoDeviceId_ in this.mirroringToggles_) {
    enabled = this.mirroringToggles_[this.videoDeviceId_];
  }
  this.toggleMirror_.toggleChecked(enabled);
};

/**
 * Saves the toggled mirror state for the current video device.
 * @private
 */
cca.views.camera.Options.prototype.saveMirroring_ = function() {
  this.mirroringToggles_[this.videoDeviceId_] = this.toggleMirror_.checked;
  cca.proxy.browserProxy.localStorageSet(
      {mirroringToggles: this.mirroringToggles_});
};

/**
 * Enables/disables the current audio track by the microphone option.
 * @private
 */
cca.views.camera.Options.prototype.updateAudioByMic_ = function() {
  if (this.audioTrack_) {
    this.audioTrack_.enabled = this.toggleMic_.checked;
  }
};

/**
 * Gets the video device ids sorted by preference.
 * @async
 * @return {Array<?string>} May contain null for user facing camera on HALv1
 *     devices.
 * @throws {Error} Throws exception for no available video devices.
 */
cca.views.camera.Options.prototype.videoDeviceIds = async function() {
  const camera3Info = await this.infoUpdater_.getCamera3DevicesInfo();

  let facings = null;
  if (camera3Info) {
    var devices = camera3Info;
    facings = camera3Info.reduce(
        (facings, info) =>
            Object.assign(facings, {[info.deviceId]: info.facing}),
        {});
    devices = camera3Info;
    this.isV1NoFacingConfig_ = false;
  } else {
    devices = await this.infoUpdater_.getDevicesInfo();
  }

  const defaultFacing =
      await cca.mojo.ChromeHelper.getInstance().isTabletMode() ?
      cros.mojom.CameraFacing.CAMERA_FACING_BACK :
      cros.mojom.CameraFacing.CAMERA_FACING_FRONT;
  // Put the selected video device id first.
  var sorted = devices.map((device) => device.deviceId).sort((a, b) => {
    if (a === b) {
      return 0;
    }
    if (this.videoDeviceId_ ? a === this.videoDeviceId_ :
                              (facings && facings[a] === defaultFacing)) {
      return -1;
    }
    return 1;
  });
  // Prepended 'null' deviceId means the system default camera on HALv1
  // device. Add it only when the app is launched (no video-device-id set).
  if (!facings && this.videoDeviceId_ === null) {
    sorted.unshift(null);
  }
  return sorted;
};
