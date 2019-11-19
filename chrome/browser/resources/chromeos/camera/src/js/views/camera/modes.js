// Copyright (c) 2019 The Chromium Authors. All rights reserved.
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

/* eslint-disable no-unused-vars */

/**
 * Contains video recording result.
 * @typedef {{
 *     resolution: {width: number, height: number},
 *     duration: number,
 *     videoSaver: !cca.models.VideoSaver,
 * }}
 */
cca.views.camera.VideoResult;

/**
 * Contains photo taking result.
 * @typedef {{
 *     resolution: {width: number, height: number},
 *     blob: !Blob,
 * }}
 */
cca.views.camera.PhotoResult;

/**
 * Callback to trigger mode switching.
 * return {!Promise}
 * @typedef {function(): !Promise}
 */
cca.views.camera.DoSwitchMode;

/**
 * Callback for saving photo capture result.
 * param {!cca.views.camera.PhotoResult} Captured photo result.
 * param {string} Name of the photo result to be saved as.
 * return {!Promise}
 * @typedef {function(cca.views.camera.PhotoResult, string): !Promise}
 */
cca.views.camera.DoSavePhoto;

/**
 * Callback for allocating VideoSaver to save video capture result.
 * @typedef {function(): !Promise<!cca.models.VideoSaver>}
 */
cca.views.camera.CreateVideoSaver;

/**
 * Callback for saving video capture result.
 * param {!cca.views.camera.VideoResult} Captured video result.
 * param {string} Name of the video result to be saved as.
 * return {!Promise}
 * @typedef {function(!cca.views.camera.VideoResult, string): !Promise}
 */
cca.views.camera.DoSaveVideo;

/**
 * Callback for playing shutter effect.
 * @typedef {function()}
 */
cca.views.camera.PlayShutterEffect;

/* eslint-enable no-unused-vars */

/**
 * Capture modes.
 * @enum {string}
 */
cca.views.camera.Mode = {
  PHOTO: 'photo-mode',
  VIDEO: 'video-mode',
  SQUARE: 'square-mode',
  PORTRAIT: 'portrait-mode',
};

/**
 * The abstract interface for the mode configuration.
 * @interface
 */
cca.views.camera.ModeConfig = class {
  /**
   * Factory function to create capture object for this mode.
   * @return {!cca.views.camera.ModeBase}
   * @abstract
   */
  captureFactory() {}

  /**
   * @param {?string} deviceId
   * @return {!Promise<boolean>} Resolves to boolean indicating whether the mode
   *     is supported by video device with specified device id.
   * @abstract
   */
  async isSupported(deviceId) {}

  /**
   * Get stream constraints for HALv1 of this mode.
   * @param {?string} deviceId
   * @return {!Promise<!Array<!MediaStreamConstraints>>}
   * @abstract
   */
  async getV1Constraints(deviceId) {}

  /* eslint-disable getter-return */

  /**
   * HALv3 constraints preferrer for this mode.
   * @return {!cca.device.ConstraintsPreferrer}
   * @abstract
   */
  get constraintsPreferrer() {}

  /**
   * Mode to be fallbacked to when fail to configure this mode.
   * @return {!cca.views.camera.Mode}
   * @abstract
   */
  get nextMode() {}

  /**
   * Capture intent of this mode.
   * @return {!cros.mojom.CaptureIntent}
   * @abstract
   */
  get captureIntent() {}

  /* eslint-enable getter-return */
};

/**
 * Mode controller managing capture sequence of different camera mode.
 */
cca.views.camera.Modes = class {
  /**
   * @param {!cca.views.camera.Mode} defaultMode Default mode to be switched to.
   * @param {!cca.device.PhotoConstraintsPreferrer} photoPreferrer
   * @param {!cca.device.VideoConstraintsPreferrer} videoPreferrer
   * @param {!cca.views.camera.DoSwitchMode} doSwitchMode
   * @param {!cca.views.camera.DoSavePhoto} doSavePhoto
   * @param {!cca.views.camera.CreateVideoSaver} createVideoSaver
   * @param {!cca.views.camera.DoSaveVideo} doSaveVideo
   * @param {!cca.views.camera.PlayShutterEffect} playShutterEffect
   */
  constructor(
      defaultMode, photoPreferrer, videoPreferrer, doSwitchMode, doSavePhoto,
      createVideoSaver, doSaveVideo, playShutterEffect) {
    /**
     * @type {!cca.views.camera.DoSwitchMode}
     * @private
     */
    this.doSwitchMode_ = doSwitchMode;

    /**
     * Capture controller of current camera mode.
     * @type {?cca.views.camera.ModeBase}
     */
    this.current = null;

    /**
     * Stream of current mode.
     * @type {?MediaStream}
     * @private
     */
    this.stream_ = null;

    /**
     * @type {!HTMLElement}
     * @private
     */
    this.modesGroup_ =
        /** @type {!HTMLElement} */ (document.querySelector('#modes-group'));

    /**
     * @type {?Resolution}
     * @private
     */
    this.captureResolution_ = null;

    /**
     * Returns a set of available constraints for HALv1 device.
     * @param {boolean} videoMode Is getting constraints for video mode.
     * @param {?string} deviceId Id of video device.
     * @return {!Promise<!Array<!MediaStreamConstraints>>} Result of
     *     constraints-candidates.
     */
    const getV1Constraints = async function(videoMode, deviceId) {
      const defaultFacing = await cca.util.getDefaultFacing();
      return [
        {
          aspectRatio: {ideal: videoMode ? 1.7777777778 : 1.3333333333},
          width: {min: 1280},
          frameRate: {min: 24},
        },
        {
          width: {min: 640},
          frameRate: {min: 24},
        },
      ].map((constraint) => {
        if (deviceId) {
          constraint.deviceId = {exact: deviceId};
        } else {
          // HALv1 devices are unable to know facing before stream
          // configuration, deviceId is set to null for requesting camera with
          // default facing.
          constraint.facingMode = {exact: defaultFacing};
        }
        return {
          audio: videoMode ? {echoCancellation: false} : false,
          video: constraint,
        };
      });
    };

    /**
     * Mode classname and related functions and attributes.
     * @type {!Object<!cca.views.camera.Mode, !cca.views.camera.ModeConfig>}
     * @private
     */
    this.allModes_ = {
      [cca.views.camera.Mode.VIDEO]: {
        captureFactory: () => new cca.views.camera.Video(
            /** @type {!MediaStream} */ (this.stream_), createVideoSaver,
            doSaveVideo),
        isSupported: async () => true,
        constraintsPreferrer: videoPreferrer,
        getV1Constraints: getV1Constraints.bind(this, true),
        nextMode: 'photo-mode',
        captureIntent: cros.mojom.CaptureIntent.VIDEO_RECORD,
      },
      [cca.views.camera.Mode.PHOTO]: {
        captureFactory: () => new cca.views.camera.Photo(
            /** @type {!MediaStream} */ (this.stream_), doSavePhoto,
            this.captureResolution_, playShutterEffect),
        isSupported: async () => true,
        constraintsPreferrer: photoPreferrer,
        getV1Constraints: getV1Constraints.bind(this, false),
        nextMode: 'square-mode',
        captureIntent: cros.mojom.CaptureIntent.STILL_CAPTURE,
      },
      [cca.views.camera.Mode.SQUARE]: {
        captureFactory: () => new cca.views.camera.Square(
            /** @type {!MediaStream} */ (this.stream_), doSavePhoto,
            this.captureResolution_, playShutterEffect),
        isSupported: async () => true,
        constraintsPreferrer: photoPreferrer,
        getV1Constraints: getV1Constraints.bind(this, false),
        nextMode: 'portrait-mode',
        captureIntent: cros.mojom.CaptureIntent.STILL_CAPTURE,
      },
      [cca.views.camera.Mode.PORTRAIT]: {
        captureFactory: () => new cca.views.camera.Portrait(
            /** @type {!MediaStream} */ (this.stream_), doSavePhoto,
            this.captureResolution_, playShutterEffect),
        isSupported: async (deviceId) => {
          if (deviceId === null) {
            return false;
          }
          const deviceOperator = await cca.mojo.DeviceOperator.getInstance();
          if (deviceOperator === null) {
            return false;
          }
          return await deviceOperator.isPortraitModeSupported(deviceId);
        },
        constraintsPreferrer: photoPreferrer,
        getV1Constraints: getV1Constraints.bind(this, false),
        nextMode: 'photo-mode',
        captureIntent: cros.mojom.CaptureIntent.STILL_CAPTURE,
      },
    };

    document.querySelectorAll('.mode-item>input').forEach((element) => {
      element.addEventListener('click', (event) => {
        if (!cca.state.get('streaming') || cca.state.get('taking')) {
          event.preventDefault();
        }
      });
      element.addEventListener('change', (event) => {
        if (element.checked) {
          var mode = element.dataset.mode;
          this.updateModeUI_(mode);
          cca.state.set('mode-switching', true);
          this.doSwitchMode_().then(
              () => cca.state.set('mode-switching', false));
        }
      });
    });

    ['expert', 'save-metadata'].forEach((state) => {
      cca.state.addObserver(state, this.updateSaveMetadata_.bind(this));
    });

    // Set default mode when app started.
    this.updateModeUI_(defaultMode);
  }

  /**
   * Updates state of mode related UI to the target mode.
   * @param {!cca.views.camera.Mode} mode Mode to be toggled.
   * @private
   */
  updateModeUI_(mode) {
    Object.keys(this.allModes_).forEach((m) => cca.state.set(m, m === mode));
    const element =
        document.querySelector(`.mode-item>input[data-mode=${mode}]`);
    element.checked = true;
    const wrapper = element.parentElement;
    let scrollTop = wrapper.offsetTop - this.modesGroup_.offsetHeight / 2 +
        wrapper.offsetHeight / 2;
    // Make photo mode scroll slightly upper so that the third mode item falls
    // in blur area: crbug.com/988869
    if (mode === 'photo-mode') {
      scrollTop -= 16;
    }
    this.modesGroup_.scrollTo({
      left: 0,
      top: scrollTop,
      behavior: 'smooth',
    });
  }

  /**
   * Gets all mode candidates. Desired trying sequence of candidate modes is
   * reflected in the order of the returned array.
   * @return {!Array<string>} Mode candidates to be tried out.
   */
  getModeCandidates() {
    const tried = {};
    const results = [];
    let mode = /** @type {!cca.views.camera.Mode} */ (
        Object.keys(this.allModes_).find(cca.state.get));
    while (!tried[mode]) {
      tried[mode] = true;
      results.push(mode);
      mode = this.allModes_[mode].nextMode;
    }
    return results;
  }

  /**
   * Gets all available capture resolution and its corresponding preview
   * constraints for the given mode.
   * @param {!cca.views.camera.Mode} mode
   * @param {string} deviceId
   * @param {!ResolutionList} previewResolutions
   * @return {!Array<!CaptureCandidate>}
   */
  getResolutionCandidates(mode, deviceId, previewResolutions) {
    return this.allModes_[mode].constraintsPreferrer.getSortedCandidates(
        deviceId, previewResolutions);
  }

  /**
   * Gets capture resolution and its corresponding preview constraints for the
   * given mode on camera HALv1 device.
   * @param {!cca.views.camera.Mode} mode
   * @param {?string} deviceId
   * @return {!Promise<!Array<!CaptureCandidate>>}
   */
  async getResolutionCandidatesV1(mode, deviceId) {
    const previewCandidates =
        await this.allModes_[mode].getV1Constraints(deviceId);
    return [{resolution: null, previewCandidates}];
  }

  /**
   * Gets capture intent for the given mode.
   * @param {!cca.views.camera.Mode} mode
   * @return {cros.mojom.CaptureIntent} Capture intent for the given mode.
   */
  getCaptureIntent(mode) {
    return this.allModes_[mode].captureIntent;
  }

  /**
   * Gets supported modes for video device of given device id.
   * @param {?string} deviceId Device id of the video device.
   * @return {!Promise<!Array<!cca.views.camera.Mode>>} All supported mode for
   *     the video device.
   */
  async getSupportedModes(deviceId) {
    let supportedModes = [];
    for (const [mode, obj] of Object.entries(this.allModes_)) {
      if (await obj.isSupported(deviceId)) {
        supportedModes.push(mode);
      }
    }
    return supportedModes;
  }

  /**
   * Updates mode selection UI according to given device id.
   * @param {?string} deviceId
   * @return {!Promise}
   */
  async updateModeSelectionUI(deviceId) {
    const supportedModes = await this.getSupportedModes(deviceId);
    document.querySelectorAll('.mode-item').forEach((element) => {
      const radio = element.querySelector('input[type=radio]');
      element.classList.toggle(
          'hide', !supportedModes.includes(radio.dataset.mode));
    });
    this.modesGroup_.classList.toggle('scrollable', supportedModes.length > 3);
    this.modesGroup_.classList.remove('hide');
  }

  /**
   * Creates and updates new current mode object.
   * @param {!cca.views.camera.Mode} mode Classname of mode to be updated.
   * @param {!MediaStream} stream Stream of the new switching mode.
   * @param {?string} deviceId Device id of currently working video device.
   * @param {?Resolution} captureResolution Capturing resolution width and
   *     height.
   * @return {!Promise}
   */
  async updateMode(mode, stream, deviceId, captureResolution) {
    if (this.current !== null) {
      await this.current.stopCapture();
    }
    this.updateModeUI_(mode);
    this.stream_ = stream;
    this.captureResolution_ = captureResolution;
    this.current = this.allModes_[mode].captureFactory();
    if (deviceId && this.captureResolution_) {
      this.allModes_[mode].constraintsPreferrer.updateValues(
          deviceId, stream, this.captureResolution_);
    }
    await this.updateSaveMetadata_();
  }

  /**
   * Checks whether to save image metadata or not.
   * @return {!Promise} Promise for the operation.
   * @private
   */
  async updateSaveMetadata_() {
    if (cca.state.get('expert') && cca.state.get('save-metadata')) {
      await this.enableSaveMetadata_();
    } else {
      await this.disableSaveMetadata_();
    }
  }

  /**
   * Enables save metadata of subsequent photos in the current mode.
   * @return {!Promise} Promise for the operation.
   * @private
   */
  async enableSaveMetadata_() {
    if (this.current !== null) {
      await this.current.addMetadataObserver();
    }
  }

  /**
   * Disables save metadata of subsequent photos in the current mode.
   * @return {!Promise} Promise for the operation.
   * @private
   */
  async disableSaveMetadata_() {
    if (this.current !== null) {
      await this.current.removeMetadataObserver();
    }
  }
};

/**
 * Base class for controlling capture sequence in different camera modes.
 * @abstract
 */
cca.views.camera.ModeBase = class {
  /**
   * @param {!MediaStream} stream
   * @param {?Resolution} captureResolution Capturing resolution width and
   *     height.
   */
  constructor(stream, captureResolution) {
    /**
     * Stream of current mode.
     * @type {!MediaStream}
     * @protected
     */
    this.stream_ = stream;

    /**
     * Capture resolution. May be null on device not support of setting
     * resolution.
     * @type {?Resolution}
     * @private
     */
    this.captureResolution_ = captureResolution;

    /**
     * Promise for ongoing capture operation.
     * @type {?Promise}
     * @private
     */
    this.capture_ = null;
  }

  /**
   * Initiates video/photo capture operation.
   * @return {!Promise} Promise for ongoing capture operation.
   */
  startCapture() {
    if (this.capture_ === null) {
      this.capture_ = this.start_().finally(() => this.capture_ = null);
    }
    return this.capture_;
  }

  /**
   * Stops the ongoing capture operation.
   * @return {!Promise} Promise for ongoing capture operation.
   */
  async stopCapture() {
    this.stop_();
    return await this.capture_;
  }

  /**
   * Adds an observer to save image metadata.
   * @return {!Promise} Promise for the operation.
   */
  async addMetadataObserver() {}

  /**
   * Remove the observer that saves metadata.
   * @return {!Promise} Promise for the operation.
   */
  async removeMetadataObserver() {}

  /**
   * Initiates video/photo capture operation under this mode.
   * @return {!Promise}
   * @protected
   * @abstract
   */
  async start_() {}

  /**
   * Stops the ongoing capture operation under this mode.
   * @protected
   */
  stop_() {}
};

/**
 * Video mode capture controller.
 * @constructor
 * @extends {cca.views.camera.ModeBase}
 */
cca.views.camera.Video = class extends cca.views.camera.ModeBase {
  /**
   * @param {!MediaStream} stream
   * @param {!cca.views.camera.CreateVideoSaver} createVideoSaver
   * @param {!cca.views.camera.DoSaveVideo} doSaveVideo
   */
  constructor(stream, createVideoSaver, doSaveVideo) {
    super(stream, null);

    /**
     * @type {!cca.views.camera.CreateVideoSaver}
     * @private
     */
    this.createVideoSaver_ = createVideoSaver;

    /**
     * @type {!cca.views.camera.DoSaveVideo}
     * @private
     */
    this.doSaveVideo_ = doSaveVideo;

    /**
     * Promise for play start sound delay.
     * @type {?Promise}
     * @private
     */
    this.startSound_ = null;

    /**
     * MediaRecorder object to record motion pictures.
     * @type {?MediaRecorder}
     * @private
     */
    this.mediaRecorder_ = null;

    /**
     * Record-time for the elapsed recording time.
     * @type {!cca.views.camera.RecordTime}
     * @private
     */
    this.recordTime_ = new cca.views.camera.RecordTime();
  }

  /**
   * @override
   */
  async start_() {
    this.startSound_ = cca.sound.play('#sound-rec-start');
    try {
      await this.startSound_;
    } finally {
      this.startSound_ = null;
    }

    if (this.mediaRecorder_ === null) {
      try {
        if (!MediaRecorder.isTypeSupported(
                cca.views.camera.Video.VIDEO_MIMETYPE)) {
          throw new Error('The preferred mimeType is not supported.');
        }
        this.mediaRecorder_ = new MediaRecorder(
            this.stream_, {mimeType: cca.views.camera.Video.VIDEO_MIMETYPE});
      } catch (e) {
        cca.toast.show('error_msg_record_start_failed');
        throw e;
      }
    }

    this.recordTime_.start();
    try {
      var videoSaver = await this.captureVideo_();
    } catch (e) {
      cca.toast.show('error_msg_empty_recording');
      throw e;
    } finally {
      var duration = this.recordTime_.stop();
    }
    cca.sound.play('#sound-rec-end');

    const {width, height} = this.stream_.getVideoTracks()[0].getSettings();
    await this.doSaveVideo_(
        {resolution: {width, height}, duration, videoSaver},
        (new cca.models.Filenamer()).newVideoName());
  }

  /**
   * @override
   */
  stop_() {
    if (this.startSound_ && this.startSound_.cancel) {
      this.startSound_.cancel();
    }
    if (this.mediaRecorder_ && this.mediaRecorder_.state === 'recording') {
      this.mediaRecorder_.stop();
    }
  }

  /**
   * Starts recording and waits for stop recording event triggered by stop
   * shutter.
   * @return {!Promise<!cca.models.VideoSaver>} Saves recorded video.
   * @private
   */
  async captureVideo_() {
    const saver = await this.createVideoSaver_();

    return new Promise((resolve, reject) => {
      let noChunk = true;

      var ondataavailable = (event) => {
        if (event.data && event.data.size > 0) {
          noChunk = false;
          saver.write(event.data);
        }
      };
      var onstop = (event) => {
        this.mediaRecorder_.removeEventListener(
            'dataavailable', ondataavailable);
        this.mediaRecorder_.removeEventListener('stop', onstop);

        if (noChunk) {
          reject(new Error('Video blob error.'));
        } else {
          // TODO(yuli): Handle insufficient storage.
          resolve(saver);
        }
      };
      this.mediaRecorder_.addEventListener('dataavailable', ondataavailable);
      this.mediaRecorder_.addEventListener('stop', onstop);
      this.mediaRecorder_.start(3000);
    });
  }
};

/**
 * Video recording MIME type. Mkv with AVC1 is the only preferred
 * format.
 * @type {string}
 * @const
 */
cca.views.camera.Video.VIDEO_MIMETYPE = 'video/x-matroska;codecs=avc1';

/**
 * Photo mode capture controller.
 */
cca.views.camera.Photo = class extends cca.views.camera.ModeBase {
  /**
   * @param {!MediaStream} stream
   * @param {!cca.views.camera.DoSavePhoto} doSavePhoto
   * @param {?Resolution} captureResolution
   * @param {!cca.views.camera.PlayShutterEffect} playShutterEffect
   */
  constructor(stream, doSavePhoto, captureResolution, playShutterEffect) {
    super(stream, captureResolution);

    /**
     * Callback for saving picture.
     * @type {!cca.views.camera.DoSavePhoto}
     * @protected
     */
    this.doSavePhoto_ = doSavePhoto;

    /**
     * ImageCapture object to capture still photos.
     * @type {?cca.mojo.ImageCapture}
     * @private
     */
    this.crosImageCapture_ = null;

    /**
     * The observer id for saving metadata.
     * @type {?number}
     * @private
     */
    this.metadataObserverId_ = null;

    /**
     * Metadata names ready to be saved.
     * @type {!Array<string>}
     * @private
     */
    this.metadataNames_ = [];

    /**
     * Callback for playing shutter effect.
     * @type {!cca.views.camera.PlayShutterEffect}
     * @protected
     */
    this.playShutterEffect_ = playShutterEffect;
  }

  /**
   * @override
   */
  async start_() {
    if (this.crosImageCapture_ === null) {
      this.crosImageCapture_ =
          new cca.mojo.ImageCapture(this.stream_.getVideoTracks()[0]);
    }

    const imageName = (new cca.models.Filenamer()).newImageName();
    if (this.metadataObserverId_ !== null) {
      this.metadataNames_.push(cca.models.Filenamer.getMetadataName(imageName));
    }

    try {
      var result = await this.createPhotoResult_();
    } catch (e) {
      cca.toast.show('error_msg_take_photo_failed');
      throw e;
    }
    await this.doSavePhoto_(result, imageName);
  }

  /**
   * Takes a photo and returns capture result.
   * @return {!Promise<!cca.views.camera.PhotoResult>} Image capture result.
   * @private
   */
  async createPhotoResult_() {
    let photoSettings;
    if (this.captureResolution_) {
      photoSettings = /** @type {!PhotoSettings} */ ({
        imageWidth: this.captureResolution_.width,
        imageHeight: this.captureResolution_.height,
      });
    } else {
      const caps = await this.crosImageCapture_.getPhotoCapabilities();
      photoSettings = /** @type {!PhotoSettings} */ ({
        imageWidth: caps.imageWidth.max,
        imageHeight: caps.imageHeight.max,
      });
    }

    try {
      const results = await this.crosImageCapture_.takePhoto(photoSettings);
      this.playShutterEffect_();
      const blob = await results[0];
      const {width, height} = await cca.util.blobToImage(blob);
      return {resolution: {width, height}, blob};
    } catch (e) {
      cca.toast.show('error_msg_take_photo_failed');
      throw e;
    }
  }

  /**
   * Adds an observer to save metadata.
   * @return {!Promise} Promise for the operation.
   */
  async addMetadataObserver() {
    if (!this.stream_) {
      return;
    }

    const deviceOperator = await cca.mojo.DeviceOperator.getInstance();
    if (!deviceOperator) {
      return;
    }

    const cameraMetadataTagInverseLookup = {};
    Object.entries(cros.mojom.CameraMetadataTag).forEach(([key, value]) => {
      if (key === 'MIN_VALUE' || key === 'MAX_VALUE') {
        return;
      }
      cameraMetadataTagInverseLookup[value] = key;
    });

    const callback = (metadata) => {
      const parsedMetadata = {};
      for (const entry of metadata.entries) {
        const key = cameraMetadataTagInverseLookup[entry.tag];
        if (key === undefined) {
          // TODO(kaihsien): Add support for vendor tags.
          continue;
        }

        const val = cca.mojo.parseMetadataData(entry);
        parsedMetadata[key] = val;
      }

      cca.models.FileSystem.saveBlob(
          new Blob(
              [JSON.stringify(parsedMetadata, null, 2)],
              {type: 'application/json'}),
          this.metadataNames_.shift());
    };

    const deviceId = this.stream_.getVideoTracks()[0].getSettings().deviceId;
    this.metadataObserverId_ = await deviceOperator.addMetadataObserver(
        deviceId, callback, cros.mojom.StreamType.JPEG_OUTPUT);
  }

  /**
   * Removes the observer that saves metadata.
   * @return {!Promise} Promise for the operation.
   */
  async removeMetadataObserver() {
    if (!this.stream_ || this.metadataObserverId_ === null) {
      return;
    }

    const deviceOperator = await cca.mojo.DeviceOperator.getInstance();
    if (!deviceOperator) {
      return;
    }

    const deviceId = this.stream_.getVideoTracks()[0].getSettings().deviceId;
    const isSuccess = await deviceOperator.removeMetadataObserver(
        deviceId, this.metadataObserverId_);
    if (!isSuccess) {
      console.error(`Failed to remove metadata observer with id: ${
          this.metadataObserverId_}`);
    }
    this.metadataObserverId_ = null;
  }
};

/**
 * Square mode capture controller.
 */
cca.views.camera.Square = class extends cca.views.camera.Photo {
  /**
   * @param {!MediaStream} stream
   * @param {!cca.views.camera.DoSavePhoto} doSavePhoto
   * @param {?Resolution} captureResolution
   * @param {!cca.views.camera.PlayShutterEffect} playShutterEffect
   */
  constructor(stream, doSavePhoto, captureResolution, playShutterEffect) {
    super(stream, doSavePhoto, captureResolution, playShutterEffect);

    this.doSavePhoto_ = async (result, ...args) => {
      // Since the image blob after square cut will lose its EXIF including
      // orientation information. Corrects the orientation before the square
      // cut.
      result.blob = await new Promise(
          (resolve, reject) =>
              cca.util.orientPhoto(result.blob, resolve, reject));
      result.blob = await this.cropSquare(result.blob);
      await doSavePhoto(result, ...args);
    };
  }

  /**
   * Crops out maximum possible centered square from the image blob.
   * @param {!Blob} blob
   * @return {!Promise<!Blob>} Promise with result cropped square image.
   */
  async cropSquare(blob) {
    const img = await cca.util.blobToImage(blob);
    let side = Math.min(img.width, img.height);
    let canvas = document.createElement('canvas');
    canvas.width = side;
    canvas.height = side;
    let ctx = canvas.getContext('2d');
    ctx.drawImage(
        img, Math.floor((img.width - side) / 2),
        Math.floor((img.height - side) / 2), side, side, 0, 0, side, side);
    const croppedBlob = await new Promise((resolve) => {
      canvas.toBlob(resolve, 'image/jpeg');
    });
    croppedBlob.resolution = blob.resolution;
    return croppedBlob;
  }
};

/**
 * Portrait mode capture controller.
 */
cca.views.camera.Portrait = class extends cca.views.camera.Photo {
  /**
   * @param {!MediaStream} stream
   * @param {!cca.views.camera.DoSavePhoto} doSavePhoto
   * @param {?Resolution} captureResolution
   * @param {!cca.views.camera.PlayShutterEffect} playShutterEffect
   */
  constructor(stream, doSavePhoto, captureResolution, playShutterEffect) {
    super(stream, doSavePhoto, captureResolution, playShutterEffect);
  }

  /**
   * @override
   */
  async start_() {
    if (this.crosImageCapture_ === null) {
      this.crosImageCapture_ =
          new cca.mojo.ImageCapture(this.stream_.getVideoTracks()[0]);
    }

    let photoSettings;
    if (this.captureResolution_) {
      photoSettings = /** @type {!PhotoSettings} */ ({
        imageWidth: this.captureResolution_.width,
        imageHeight: this.captureResolution_.height,
      });
    } else {
      const caps = await this.crosImageCapture_.getPhotoCapabilities();
      photoSettings = /** @type {!PhotoSettings} */ ({
        imageWidth: caps.imageWidth.max,
        imageHeight: caps.imageHeight.max,
      });
    }

    const filenamer = new cca.models.Filenamer();
    const refImageName = filenamer.newBurstName(false);
    const portraitImageName = filenamer.newBurstName(true);

    if (this.metadataObserverId_ !== null) {
      [refImageName, portraitImageName].forEach((imageName) => {
        this.metadataNames_.push(
            cca.models.Filenamer.getMetadataName(imageName));
      });
    }

    try {
      var [reference, portrait] = await this.crosImageCapture_.takePhoto(
          photoSettings, [cros.mojom.Effect.PORTRAIT_MODE]);
      this.playShutterEffect_();
    } catch (e) {
      cca.toast.show('error_msg_take_photo_failed');
      throw e;
    }

    const [refSave, portraitSave] = [
      [reference, refImageName],
      [portrait, portraitImageName],
    ].map(async ([p, imageName]) => {
      const isPortrait = Object.is(p, portrait);
      try {
        var blob = await p;
      } catch (e) {
        cca.toast.show(
            isPortrait ? 'error_msg_take_portrait_photo_failed' :
                         'error_msg_take_photo_failed');
        throw e;
      }
      const {width, height} = await cca.util.blobToImage(blob);
      await this.doSavePhoto_({resolution: {width, height}, blob}, imageName);
    });
    try {
      await portraitSave;
    } catch (e) {
      // Portrait image may failed due to absence of human faces.
      // TODO(inker): Log non-intended error.
    }
    await refSave;
  }
};
