// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// eslint-disable-next-line no-unused-vars
import {assertNotReached} from '../assert.js';
import {
  PhotoConstraintsPreferrer,  // eslint-disable-line no-unused-vars
  VideoConstraintsPreferrer,  // eslint-disable-line no-unused-vars
} from '../device/constraints_preferrer.js';
// eslint-disable-next-line no-unused-vars
import {DeviceInfoUpdater} from '../device/device_info_updater.js';
import {I18nString} from '../i18n_string.js';
// eslint-disable-next-line no-unused-vars
import {Intent} from '../intent.js';
import * as metrics from '../metrics.js';
// eslint-disable-next-line no-unused-vars
import {FileAccessEntry} from '../models/file_system_access_entry.js';
// eslint-disable-next-line no-unused-vars
import {ResultSaver} from '../models/result_saver.js';
import {VideoSaver} from '../models/video_saver.js';
// eslint-disable-next-line no-unused-vars
import {PerfLogger} from '../perf.js';
import * as state from '../state.js';
import {scaleImage} from '../thumbnailer.js';
import * as toast from '../toast.js';
// eslint-disable-next-line no-unused-vars
import {Mode} from '../type.js';
import * as util from '../util.js';

import {Camera} from './camera.js';
// eslint-disable-next-line no-unused-vars
import {PhotoResult, VideoResult} from './camera/mode/index.js';
import {ReviewResult} from './camera/review_result.js';

/**
 * The maximum number of pixels in the downscaled intent photo result. Reference
 * from GCA: https://goto.google.com/gca-inline-bitmap-max-pixel-num
 * @type {number}
 * @const
 */
const DOWNSCALE_INTENT_MAX_PIXEL_NUM = 50 * 1024;

/**
 * Camera-intent-view controller.
 */
export class CameraIntent extends Camera {
  /**
   * @param {!Intent} intent
   * @param {!DeviceInfoUpdater} infoUpdater
   * @param {!PhotoConstraintsPreferrer} photoPreferrer
   * @param {!VideoConstraintsPreferrer} videoPreferrer
   * @param {!Mode} mode
   * @param {!PerfLogger} perfLogger
   */
  constructor(
      intent, infoUpdater, photoPreferrer, videoPreferrer, mode, perfLogger) {
    const resultSaver = /** @type {!ResultSaver} */ ({
      savePhoto: async (blob) => {
        if (intent.shouldDownScale) {
          const image = await util.blobToImage(blob);
          const ratio = Math.sqrt(
              DOWNSCALE_INTENT_MAX_PIXEL_NUM / (image.width * image.height));
          blob = await scaleImage(
              blob, Math.floor(image.width * ratio),
              Math.floor(image.height * ratio));
        }
        const buf = await blob.arrayBuffer();
        await this.intent_.appendData(new Uint8Array(buf));
      },
      startSaveVideo: async (outputVideoRotation) => {
        return VideoSaver.createForIntent(intent, outputVideoRotation);
      },
      finishSaveVideo: async (video) => {
        this.videoResultFile_ = await video.endWrite();
      },
      saveGif: () => {
        assertNotReached();
      },
    });
    super(
        resultSaver, infoUpdater, photoPreferrer, videoPreferrer, mode,
        perfLogger, /* facing= */ null);

    /**
     * @type {!Intent}
     * @private
     */
    this.intent_ = intent;

    /**
     * @type {?PhotoResult}
     * @private
     */
    this.photoResult_ = null;

    /**
     * @type {?VideoResult}
     * @private
     */
    this.videoResult_ = null;

    /**
     * @type {?FileAccessEntry}
     * @private
     */
    this.videoResultFile_ = null;

    /**
     * @type {!ReviewResult}
     * @private
     */
    this.reviewResult_ = new ReviewResult();
  }

  /**
   * @override
   */
  async handleResultPhoto(result, name) {
    this.photoResult_ = result;
    try {
      await this.resultSaver_.savePhoto(result.blob, name);
    } catch (e) {
      toast.show(I18nString.ERROR_MSG_SAVE_FILE_FAILED);
      throw e;
    }
  }

  /**
   * @override
   */
  async handleResultVideo(result) {
    this.videoResult_ = result;
    try {
      await this.resultSaver_.finishSaveVideo(result.videoSaver);
    } catch (e) {
      toast.show(I18nString.ERROR_MSG_SAVE_FILE_FAILED);
      throw e;
    }
  }

  /**
   * @override
   */
  beginTake(shutterType) {
    // TODO(inker): Clean unused photo result blob properly.
    this.photoResult_ = null;
    this.videoResult_ = null;

    const take = super.beginTake(shutterType);
    if (take === null) {
      return null;
    }
    return (async () => {
      await take;
      if (this.photoResult_ === null && this.videoResultFile_ === null) {
        // In case of take early finish without any result e.g. Timer canceled.
        return;
      }

      state.set(state.State.SUSPEND, true);
      await this.start();
      const confirmed = await (() => {
        if (this.photoResult_ !== null) {
          return this.reviewResult_.openPhoto(this.photoResult_.blob);
        } else if (this.videoResultFile_ !== null) {
          return this.reviewResult_.openVideo(this.videoResultFile_);
        } else {
          assertNotReached('None of intent result.');
        }
      })();
      const {resolution} = this.photoResult_ || this.videoResult_;
      const duration =
          this.videoResult_ === null ? undefined : this.videoResult_.duration;
      metrics.sendCaptureEvent({
        facing: this.facingMode_,
        duration,
        resolution,
        intentResult: confirmed ? metrics.IntentResultType.CONFIRMED :
                                  metrics.IntentResultType.CANCELED,
        shutterType: this.shutterType_,
      });
      if (confirmed) {
        await this.intent_.finish();

        const appWindow = window.appWindow;
        if (appWindow === null) {
          window.close();
        } else {
          // For test session, we notify tests and let test close the window for
          // us.
          await appWindow.notifyClosingItself();
        }
        return;
      }
      this.focus();  // Refocus the visible shutter button for ChromeVox.
      state.set(state.State.SUSPEND, false);
      await this.intent_.clearData();
      await this.start();
    })();
  }

  /**
   * @override
   */
  async startWithDevice_(deviceId) {
    return this.startWithMode_(deviceId, this.defaultMode_);
  }
}
