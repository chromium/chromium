// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-camera' is a Polymer element used to take a picture from the
 * user webcam to use as a Chrome OS profile picture.
 */
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/polymer/v3_0/iron-a11y-keys/iron-a11y-keys.js';
import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './icons.html.js';

import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_camera.html.js';
import * as webcamUtils from './webcam_utils.js';

Polymer({
  is: 'cr-camera',

  _template: getTemplate(),

  properties: {
    /** Strings provided by host */
    takePhotoLabel: String,
    captureVideoLabel: String,
    switchModeToCameraLabel: String,
    switchModeToVideoLabel: String,

    /** True if video mode is enabled. */
    videoModeEnabled: {
      type: Boolean,
      value: false,
    },

    /**
     * True if currently in video mode.
     * @private {boolean}
     */
    videomode: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /**
     * True when the camera is actually streaming video. May be false even when
     * the camera is present and shown, but still initializing.
     * @private {boolean}
     */
    cameraOnline_: {
      type: Boolean,
      value: false,
    },
  },

  /** @private {boolean} */
  cameraStartInProgress_: false,

  /** @private {boolean} */
  cameraCaptureInProgress_: false,

  /** @override */
  attached() {
    this.$.cameraVideo.addEventListener('canplay', function() {
      this.$.userImageStreamCrop.classList.add('preview');
      this.cameraOnline_ = true;
      this.focusTakePhotoButton();
    }.bind(this));
    this.startCamera();
  },

  /** @override */
  detached() {
    this.stopCamera();
  },

  /** Only focuses the button if it's not disabled. */
  focusTakePhotoButton() {
    if (this.cameraOnline_) {
      this.$.takePhoto.focus();
    }
  },

  /**
   * Performs photo capture from the live camera stream. A 'photo-taken' event
   * will be fired as soon as captured photo is available, with the
   * 'photoDataURL' property containing the photo encoded as a data URL.
   */
  async takePhoto() {
    if (!this.cameraOnline_ || this.cameraCaptureInProgress_) {
      return;
    }
    this.cameraCaptureInProgress_ = true;
    this.$.userImageStreamCrop.classList.remove('preview');
    this.$.userImageStreamCrop.classList.add('capture');

    const numFrames = this.videomode ?
        webcamUtils.CAPTURE_DURATION_MS / webcamUtils.CAPTURE_INTERVAL_MS :
        1;

    try {
      const frames = await webcamUtils.captureFrames(
          /** @type {!HTMLVideoElement} */ (this.$.cameraVideo),
          this.getCaptureSize_(), webcamUtils.CAPTURE_INTERVAL_MS, numFrames);

      const photoDataUrl = webcamUtils.convertFramesToPng(frames);
      this.fire('photo-taken', {photoDataUrl});
    } catch (e) {
      console.error(e);
    }
    this.$.userImageStreamCrop.classList.remove('capture');
    this.cameraCaptureInProgress_ = false;
  },

  /** Tries to start the camera stream capture. */
  startCamera() {
    this.stopCamera();
    this.cameraStartInProgress_ = true;

    const successCallback = function(stream) {
      if (this.cameraStartInProgress_) {
        this.$.cameraVideo.srcObject = stream;
        this.cameraStream_ = stream;
      } else {
        webcamUtils.stopMediaTracks(stream);
      }
      this.cameraStartInProgress_ = false;
    }.bind(this);

    const errorCallback = function() {
      this.cameraOnline_ = false;
      this.cameraStartInProgress_ = false;
    }.bind(this);

    navigator.webkitGetUserMedia(
        {video: webcamUtils.kDefaultVideoConstraints}, successCallback,
        errorCallback);
  },

  /** Stops the camera stream capture if it's currently active. */
  stopCamera() {
    this.$.userImageStreamCrop.classList.remove('preview');
    this.cameraOnline_ = false;
    this.$.cameraVideo.srcObject = null;
    webcamUtils.stopMediaTracks(this.cameraStream_);
    this.cameraStream_ = null;
    // Cancel any pending getUserMedia() checks.
    this.cameraStartInProgress_ = false;
  },

  /**
   * Get the correct capture size for single photo or video mode.
   * @return {{height: number, width: number}}
   * @private
   */
  getCaptureSize_() {
    if (this.videomode) {
      /** Reduce capture size when in video mode. */
      return {
        width: webcamUtils.CAPTURE_SIZE.width / 2,
        height: webcamUtils.CAPTURE_SIZE.height / 2,
      };
    }
    return webcamUtils.CAPTURE_SIZE;
  },

  /**
   * Switch between photo and video mode.
   * @private
   */
  onTapSwitchMode_() {
    this.videomode = !this.videomode;
    this.fire('switch-mode', this.videomode);
  },

  /**
   * @return {string}
   * @private
   */
  getTakePhotoIcon_() {
    return this.videomode ? 'cr-picture:videocam-shutter-icon' :
                            'cr-picture:camera-shutter-icon';
  },

  /**
   * Returns the label to use for take photo button.
   * @return {string}
   * @private
   */
  getTakePhotoLabel_(videomode, photoLabel, videoLabel) {
    return videomode ? videoLabel : photoLabel;
  },

  /**
   * @return {string}
   * @private
   */
  getSwitchModeIcon_() {
    return this.videomode ? 'cr-picture:camera-alt-icon' :
                            'cr-picture:videocam-icon';
  },

  /**
   * Returns the label to use for switch mode button.
   * @return {string}
   * @private
   */
  getSwitchModeLabel_(videomode, cameraLabel, videoLabel) {
    return videomode ? cameraLabel : videoLabel;
  },
});
