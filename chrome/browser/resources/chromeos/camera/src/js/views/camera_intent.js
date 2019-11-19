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
 * The maximum number of pixels in the downscaled intent photo result. Reference
 * from GCA: https://goto.google.com/gca-inline-bitmap-max-pixel-num
 * @type {number}
 * @const
 */
cca.views.DOWNSCALE_INTENT_MAX_PIXEL_NUM = 50 * 1024;

/**
 * Creates the camera-intent-view controller.
 */
cca.views.CameraIntent = class extends cca.views.Camera {
  /**
   * @param {!cca.intent.Intent} intent
   * @param {!cca.device.DeviceInfoUpdater} infoUpdater
   * @param {!cca.device.PhotoConstraintsPreferrer} photoPreferrer
   * @param {!cca.device.VideoConstraintsPreferrer} videoPreferrer
   */
  constructor(intent, infoUpdater, photoPreferrer, videoPreferrer) {
    const resultSaver = {
      savePhoto: async (blob) => {
        if (intent.shouldDownScale) {
          const image = await cca.util.blobToImage(blob);
          const ratio = Math.sqrt(
              cca.views.DOWNSCALE_INTENT_MAX_PIXEL_NUM /
              (image.width * image.height));
          blob = await cca.util.scalePicture(
              image.src, false, Math.floor(image.width * ratio),
              Math.floor(image.height * ratio));
        }
        const buf = await blob.arrayBuffer();
        await this.intent_.appendData(new Uint8Array(buf));
      },
      startSaveVideo: async () => {
        return await cca.models.IntentVideoSaver.create(intent);
      },
      finishSaveVideo: async (video, savedName) => {
        this.videoResultFile_ = await video.endWrite();
      },
    };
    super(resultSaver, infoUpdater, photoPreferrer, videoPreferrer);

    /**
     * @type {!cca.intent.Intent}
     * @private
     */
    this.intent_ = intent;

    /**
     * @type {?cca.views.camera.PhotoResult}
     * @private
     */
    this.photoResult_ = null;

    /**
     * @type {?cca.views.camera.VideoResult}
     * @private
     */
    this.videoResult_ = null;

    /**
     * @type {?FileEntry}
     * @private
     */
    this.videoResultFile_ = null;

    /**
     * @type {!cca.views.camera.ReviewResult}
     * @private
     */
    this.reviewResult_ = new cca.views.camera.ReviewResult();
  }

  /**
   * @override
   */
  async doSavePhoto_(result, name) {
    this.photoResult_ = result;
    try {
      await this.resultSaver_.savePhoto(result.blob, name);
    } catch (e) {
      cca.toast.show('error_msg_save_file_failed');
      throw e;
    }
  }

  /**
   * @override
   */
  async doSaveVideo_(result, name) {
    this.videoResult_ = result;
    try {
      await this.resultSaver_.finishSaveVideo(result.videoSaver, name);
    } catch (e) {
      cca.toast.show('error_msg_save_file_failed');
      throw e;
    }
  }

  /**
   * @override
   */
  beginTake_() {
    if (this.photoResult_ !== null) {
      URL.revokeObjectURL(this.photoResult_.blob);
    }
    this.photoResult_ = null;
    this.videoResult_ = null;

    const take = super.beginTake_();
    if (take === null) {
      return null;
    }
    return (async () => {
      await take;

      if (this.photoResult_ === null && this.videoResult_ === null) {
        console.warn('End take without intent result.');
        return;
      }
      cca.state.set('suspend', true);
      await this.restart();
      const confirmed = await (
          this.photoResult_ !== null ?
              this.reviewResult_.openPhoto(this.photoResult_.blob) :
              this.reviewResult_.openVideo(this.videoResultFile_));
      const result = this.photoResult_ || this.videoResult_;
      cca.metrics.log(
          cca.metrics.Type.CAPTURE, this.facingMode_, result.duration || 0,
          result.resolution,
          confirmed ? cca.metrics.IntentResultType.CONFIRMED :
                      cca.metrics.IntentResultType.CANCELED);
      if (confirmed) {
        await this.intent_.finish();
        window.close();
        return;
      }
      this.focus();  // Refocus the visible shutter button for ChromeVox.
      cca.state.set('suspend', false);
      await this.intent_.clearData();
      await this.restart();
    })();
  }

  /**
   * @override
   */
  async startWithDevice_(deviceId) {
    return this.startWithMode_(deviceId, this.defaultMode);
  }
};
