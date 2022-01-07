// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// eslint-disable-next-line no-unused-vars
import {assert, assertNotReached} from '../assert.js';
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
import {scaleImage} from '../thumbnailer.js';
// eslint-disable-next-line no-unused-vars
import {Mode, Resolution} from '../type.js';
import * as util from '../util.js';

import {Camera} from './camera.js';
// eslint-disable-next-line no-unused-vars
import {PhotoResult, VideoResult} from './camera/mode/index.js';
import * as review from './review.js';

/**
 * The maximum number of pixels in the downscaled intent photo result. Reference
 * from GCA: https://goto.google.com/gca-inline-bitmap-max-pixel-num
 * @type {number}
 * @const
 */
const DOWNSCALE_INTENT_MAX_PIXEL_NUM = 50 * 1024;

/**
 * @typedef {{
 *   resolution: !Resolution,
 *   duration?: number,
 * }}
 */
let MetricArgs;  // eslint-disable-line no-unused-vars

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
     * @type {?FileAccessEntry}
     * @private
     */
    this.videoResultFile_ = null;
  }

  /**
   * @param {!MetricArgs} metricArgs
   * @return {!Promise<void>}
   * @private
   */
  reviewIntentResult_(metricArgs) {
    return this.prepareReview(async () => {
      const confirmed = await this.review.startReview(new review.OptionGroup({
        template: review.ButtonGroupTemplate.intent,
        options: [
          new review.Option(
              {
                label: I18nString.CONFIRM_REVIEW_BUTTON,
                templateId: 'review-intent-button-template',
              },
              {exitValue: true}),
          new review.Option(
              {
                label: I18nString.CANCEL_REVIEW_BUTTON,
                templateId: 'review-intent-button-template',
              },
              {exitValue: false}),
        ],

      }));
      metrics.sendCaptureEvent({
        facing: this.facingMode,
        ...metricArgs,
        intentResult: confirmed ? metrics.IntentResultType.CONFIRMED :
                                  metrics.IntentResultType.CANCELED,
        shutterType: this.shutterType,
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
      } else {
        await this.intent_.clearData();
      }
    });
  }

  /**
   * @override
   */
  async onPhotoCaptureDone(pendingPhotoResult) {
    await super.onPhotoCaptureDone(pendingPhotoResult);
    const {blob, resolution} = await pendingPhotoResult;
    await this.review.setReviewPhoto(blob);
    await this.reviewIntentResult_({resolution});
  }

  /**
   * @override
   */
  async onVideoCaptureDone(videoResult) {
    await super.onVideoCaptureDone(videoResult);
    assert(this.videoResultFile_ !== null);
    await this.review.setReviewVideo(this.videoResultFile_);
    await this.reviewIntentResult_(
        {resolution: videoResult.resolution, duration: videoResult.duration});
  }

  /**
   * @override
   */
  async startWithDevice(deviceId) {
    return this.startWithMode(deviceId, this.defaultMode);
  }
}
