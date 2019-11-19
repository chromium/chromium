// Copyright (c) 2013 The Chromium Authors. All rights reserved.
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
 * Thrown when app window suspended during stream reconfiguration.
 */
cca.views.CameraSuspendedError = class extends Error {
  /**
   * @param {string=} message Error message.
   */
  constructor(message = 'Camera suspended.') {
    super(message);
    this.name = 'CameraSuspendedError';
  }
};

/**
 * Creates the camera-view controller.
 * @param {!cca.models.ResultSaver} resultSaver
 * @param {!cca.device.DeviceInfoUpdater} infoUpdater
 * @param {!cca.device.PhotoConstraintsPreferrer} photoPreferrer
 * @param {!cca.device.VideoConstraintsPreferrer} videoPreferrer
 * @constructor
 */
cca.views.Camera = function(
    resultSaver, infoUpdater, photoPreferrer, videoPreferrer) {
  cca.views.View.call(this, '#camera');

  /**
   * @type {cca.device.DeviceInfoUpdater}
   * @private
   */
  this.infoUpdater_ = infoUpdater;

  /**
   * Layout handler for the camera view.
   * @type {cca.views.camera.Layout}
   * @private
   */
  this.layout_ = new cca.views.camera.Layout();

  /**
   * Video preview for the camera.
   * @type {cca.views.camera.Preview}
   * @private
   */
  this.preview_ = new cca.views.camera.Preview(this.restart.bind(this));

  /**
   * Options for the camera.
   * @type {cca.views.camera.Options}
   * @private
   */
  this.options_ =
      new cca.views.camera.Options(infoUpdater, this.restart.bind(this));

  /**
   * @type {!cca.models.ResultSaver}
   * @protected
   */
  this.resultSaver_ = resultSaver;

  /**
   * Device id of video device of active preview stream. Sets to null when
   * preview become inactive.
   * @type {?string}
   * @private
   */
  this.activeDeviceId_ = null;

  const createVideoSaver = async () => resultSaver.startSaveVideo();

  const playShutterEffect = () => {
    cca.sound.play('#sound-shutter');
    cca.util.animateOnce(this.preview_.video);
  };

  /**
   * Modes for the camera.
   * @type {cca.views.camera.Modes}
   * @private
   */
  this.modes_ = new cca.views.camera.Modes(
      this.defaultMode, photoPreferrer, videoPreferrer, this.restart.bind(this),
      this.doSavePhoto_.bind(this), createVideoSaver,
      this.doSaveVideo_.bind(this), playShutterEffect);

  /**
   * @type {?string}
   * @private
   */
  this.facingMode_ = null;

  /**
   * @type {boolean}
   * @private
   */
  this.locked_ = false;

  /**
   * @type {?number}
   * @private
   */
  this.retryStartTimeout_ = null;

  /**
   * Promise for the camera stream configuration process. It's resolved to
   * boolean for whether the configuration is failed and kick out another round
   * of reconfiguration. Sets to null once the configuration is completed.
   * @type {?Promise<boolean>}
   * @private
   */
  this.configuring_ = null;

  /**
   * Promise for the current take of photo or recording.
   * @type {?Promise}
   * @protected
   */
  this.take_ = null;

  document.querySelectorAll('#start-takephoto, #start-recordvideo')
      .forEach((btn) => btn.addEventListener('click', () => this.beginTake_()));

  document.querySelectorAll('#stop-takephoto, #stop-recordvideo')
      .forEach((btn) => btn.addEventListener('click', () => this.endTake_()));

  // Monitor the states to stop camera when locked/minimized.
  chrome.idle.onStateChanged.addListener((newState) => {
    this.locked_ = (newState === 'locked');
    if (this.locked_) {
      this.restart();
    }
  });
  chrome.app.window.current().onMinimized.addListener(() => this.restart());

  this.configuring_ = this.start_();
};

cca.views.Camera.prototype = {
  __proto__: cca.views.View.prototype,

  /**
   * Whether app window is suspended.
   * @return {boolean}
   */
  get suspended() {
    return this.locked_ || chrome.app.window.current().isMinimized() ||
        cca.state.get('suspend');
  },
  get defaultMode() {
    switch (window.intent && window.intent.mode) {
      case cca.intent.Mode.PHOTO:
        return cca.views.camera.Mode.PHOTO;
      case cca.intent.Mode.VIDEO:
        return cca.views.camera.Mode.VIDEO;
      default:
        return cca.views.camera.Mode.PHOTO;
    }
  },
};

/**
 * @override
 */
cca.views.Camera.prototype.focus = function() {
  // Avoid focusing invisible shutters.
  document.querySelectorAll('.shutter')
      .forEach((btn) => btn.offsetParent && btn.focus());
};


/**
 * Begins to take photo or recording with the current options, e.g. timer.
 * @return {?Promise} Promise resolved when take action completes. Returns null
 *     if CCA can't start take action.
 * @protected
 */
cca.views.Camera.prototype.beginTake_ = function() {
  if (!cca.state.get('streaming') || cca.state.get('taking')) {
    return null;
  }

  cca.state.set('taking', true);
  this.focus();  // Refocus the visible shutter button for ChromeVox.
  this.take_ = (async () => {
    try {
      await cca.views.camera.timertick.start();
      await this.modes_.current.startCapture();
    } catch (e) {
      if (e && e.message === 'cancel') {
        return;
      }
      console.error(e);
    } finally {
      this.take_ = null;
      cca.state.set('taking', false);
      this.focus();  // Refocus the visible shutter button for ChromeVox.
    }
  })();
  return this.take_;
};

/**
 * Ends the current take (or clears scheduled further takes if any.)
 * @return {!Promise} Promise for the operation.
 * @private
 */
cca.views.Camera.prototype.endTake_ = function() {
  cca.views.camera.timertick.cancel();
  this.modes_.current.stopCapture();
  return Promise.resolve(this.take_);
};

/**
 * Handles captured photo result.
 * @param {!cca.views.camera.PhotoResult} result Captured photo result.
 * @param {string} name Name of the photo result to be saved as.
 * @return {!Promise} Promise for the operation.
 * @protected
 */
cca.views.Camera.prototype.doSavePhoto_ = async function(result, name) {
  cca.metrics.log(
      cca.metrics.Type.CAPTURE, this.facingMode_, /* length= */ 0,
      result.resolution, cca.metrics.IntentResultType.NOT_INTENT);
  try {
    await this.resultSaver_.savePhoto(result.blob, name);
  } catch (e) {
    cca.toast.show('error_msg_save_file_failed');
    throw e;
  }
};

/**
 * Handles captured video result.
 * @param {!cca.views.camera.VideoResult} result Captured video result.
 * @param {string} name Name of the video result to be saved as.
 * @return {!Promise} Promise for the operation.
 * @protected
 */
cca.views.Camera.prototype.doSaveVideo_ = async function(result, name) {
  cca.metrics.log(
      cca.metrics.Type.CAPTURE, this.facingMode_, result.duration,
      result.resolution, cca.metrics.IntentResultType.NOT_INTENT);
  try {
    await this.resultSaver_.finishSaveVideo(result.videoSaver, name);
  } catch (e) {
    cca.toast.show('error_msg_save_file_failed');
    throw e;
  }
};

/**
 * @override
 */
cca.views.Camera.prototype.layout = function() {
  this.layout_.update();
};

/**
 * @override
 */
cca.views.Camera.prototype.handlingKey = function(key) {
  if (key === 'Ctrl-R') {
    cca.toast.show(this.preview_.toString());
    return true;
  }
  return false;
};

/**
 * Stops camera and tries to start camera stream again if possible.
 * @return {!Promise<boolean>} Promise resolved to whether restart camera
 *     successfully.
 */
cca.views.Camera.prototype.restart = async function() {
  // To prevent multiple callers enter this function at the same time, wait
  // until previous caller resets configuring to null.
  while (this.configuring_ !== null) {
    if (!await this.configuring_) {
      // Retry will be kicked out soon.
      return false;
    }
  }
  this.configuring_ = (async () => {
    try {
      if (cca.state.get('taking')) {
        await this.endTake_();
      }
    } finally {
      this.preview_.stop();
    }
    return this.start_();
  })();
  return this.configuring_;
};

/**
 * Try start stream reconfiguration with specified mode and device id.
 * @param {?string} deviceId
 * @param {string} mode
 * @return {!Promise<boolean>} If found suitable stream and reconfigure
 *     successfully.
 */
cca.views.Camera.prototype.startWithMode_ = async function(deviceId, mode) {
  const deviceOperator = await cca.mojo.DeviceOperator.getInstance();
  let resolCandidates = null;
  if (deviceOperator !== null) {
    if (deviceId !== null) {
      const previewRs =
          (await this.infoUpdater_.getDeviceResolutions(deviceId)).video;
      resolCandidates =
          this.modes_.getResolutionCandidates(mode, deviceId, previewRs);
    } else {
      console.error('Null device id present on HALv3 device. Fallback to v1.');
    }
  }
  if (resolCandidates === null) {
    resolCandidates =
        await this.modes_.getResolutionCandidatesV1(mode, deviceId);
  }
  for (const {resolution: captureR, previewCandidates} of resolCandidates) {
    for (const constraints of previewCandidates) {
      if (this.suspended) {
        throw new cca.views.CameraSuspendedError();
      }
      try {
        if (deviceOperator !== null) {
          await deviceOperator.setFpsRange(deviceId, constraints);
          await deviceOperator.setCaptureIntent(
              deviceId, this.modes_.getCaptureIntent(mode));
        }
        const stream = await navigator.mediaDevices.getUserMedia(constraints);
        await this.preview_.start(stream);
        this.facingMode_ = await this.options_.updateValues(stream);
        await this.modes_.updateModeSelectionUI(deviceId);
        await this.modes_.updateMode(mode, stream, deviceId, captureR);
        cca.nav.close('warning', 'no-camera');
        return true;
      } catch (e) {
        this.preview_.stop();
        console.error(e);
      }
    }
  }
  return false;
};

/**
 * Try start stream reconfiguration with specified device id.
 * @param {?string} deviceId
 * @return {!Promise<boolean>} If found suitable stream and reconfigure
 *     successfully.
 */
cca.views.Camera.prototype.startWithDevice_ = async function(deviceId) {
  const supportedModes = await this.modes_.getSupportedModes(deviceId);
  const modes =
      this.modes_.getModeCandidates().filter((m) => supportedModes.includes(m));
  for (const mode of modes) {
    if (await this.startWithMode_(deviceId, mode)) {
      return true;
    }
  }
  return false;
};

/**
 * Starts camera configuration process.
 * @return {!Promise<boolean>} Resolved to boolean for whether the configuration
 *     is succeeded or kicks out another round of reconfiguration.
 * @private
 */
cca.views.Camera.prototype.start_ = async function() {
  try {
    await this.infoUpdater_.lockDeviceInfo(async () => {
      if (!this.suspended) {
        for (const id of await this.options_.videoDeviceIds()) {
          if (await this.startWithDevice_(id)) {
            // Make the different active camera announced by screen reader.
            const currentId = this.options_.currentDeviceId;
            if (currentId === this.activeDeviceId_) {
              return;
            }
            this.activeDeviceId_ = currentId;
            const info = await this.infoUpdater_.getDeviceInfo(id);
            if (info !== null) {
              cca.toast.speak(chrome.i18n.getMessage(
                  'status_msg_camera_switched', info.label));
            }
            return;
          }
        }
      }
      throw new cca.views.CameraSuspendedError();
    });
    this.configuring_ = null;
    return true;
  } catch (error) {
    this.activeDeviceId_ = null;
    if (!(error instanceof cca.views.CameraSuspendedError)) {
      console.error(error);
      cca.nav.open('warning', 'no-camera');
    }
    // Schedule to retry.
    if (this.retryStartTimeout_) {
      clearTimeout(this.retryStartTimeout_);
      this.retryStartTimeout_ = null;
    }
    this.retryStartTimeout_ = setTimeout(() => {
      this.configuring_ = this.start_();
    }, 100);
    return false;
  }
};
