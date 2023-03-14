// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from '../assert.js';
import {CameraManager, PhotoResult, VideoResult} from '../device/index.js';
import {I18nString} from '../i18n_string.js';
import {Intent} from '../intent.js';
import * as metrics from '../metrics.js';
import {FileAccessEntry} from '../models/file_system_access_entry.js';
import {VideoSaver} from '../models/video_saver.js';
import {ChromeHelper} from '../mojo/chrome_helper.js';
import {PerfLogger} from '../perf.js';
import {scaleImage} from '../thumbnailer.js';
import {Resolution} from '../type.js';
import * as util from '../util.js';

import {Camera} from './camera.js';
import * as review from './review.js';

/**
 * The maximum number of pixels in the downscaled intent photo result. Reference
 * from GCA: https://goto.google.com/gca-inline-bitmap-max-pixel-num.
 */
const DOWNSCALE_INTENT_MAX_PIXEL_NUM = 50 * 1024;

interface MetricArgs {
  resolution: Resolution;
  duration?: number;
}

/**
 * Camera-intent-view controller.
 */
export class CameraIntent extends Camera {
  private videoResultFile: FileAccessEntry|null = null;

  constructor(
      private readonly intent: Intent,
      cameraManager: CameraManager,
      perfLogger: PerfLogger,
  ) {
    super(
        {
          savePhoto: async (blob) => {
            if (intent.shouldDownScale) {
              const image = await util.blobToImage(blob);
              const ratio = Math.sqrt(
                  DOWNSCALE_INTENT_MAX_PIXEL_NUM /
                  (image.width * image.height));
              blob = await scaleImage(
                  blob, Math.floor(image.width * ratio),
                  Math.floor(image.height * ratio));
            }
            const buf = await blob.arrayBuffer();
            await this.intent.appendData(new Uint8Array(buf));
          },
          startSaveVideo: async (outputVideoRotation) => {
            return VideoSaver.createForIntent(intent, outputVideoRotation);
          },
          finishSaveVideo: async (video) => {
            assert(video instanceof VideoSaver);
            this.videoResultFile = await video.endWrite();
          },
          saveGif: () => {
            assertNotReached();
          },
        },
        cameraManager, perfLogger);
  }

  private reviewIntentResult(metricArgs: MetricArgs): Promise<void> {
    return this.prepareReview(async () => {
      const confirmed = await this.review.startReview(new review.OptionGroup({
        template: review.ButtonGroupTemplate.INTENT,
        options: [
          new review.Option(
              {
                label: I18nString.CONFIRM_REVIEW_BUTTON,
                templateId: 'review-intent-button-template',
                primary: true,
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
        facing: this.getFacing(),
        ...metricArgs,
        intentResult: confirmed ? metrics.IntentResultType.CONFIRMED :
                                  metrics.IntentResultType.CANCELED,
        shutterType: this.shutterType,
        resolutionLevel:
            this.cameraManager.getPhotoResolutionLevel(metricArgs.resolution),
        aspectRatioSet:
            this.cameraManager.getAspectRatioSet(metricArgs.resolution),
      });
      if (confirmed) {
        await this.intent.finish();
        const appWindow = window.appWindow;
        if (appWindow === null) {
          window.close();
        } else {
          // For test session, we notify tests and let test close the window for
          // us.
          await appWindow.notifyClosingItself();
        }
      } else {
        await this.intent.clearData();
      }
    });
  }

  override async onPhotoCaptureDone(pendingPhotoResult: Promise<PhotoResult>):
      Promise<void> {
    await super.onPhotoCaptureDone(pendingPhotoResult);
    const {blob, resolution} = await pendingPhotoResult;
    await this.review.setReviewPhoto(blob);
    await this.reviewIntentResult({resolution});
    ChromeHelper.getInstance().maybeTriggerSurvey();
  }

  override async onVideoCaptureDone(videoResult: VideoResult): Promise<void> {
    await super.onVideoCaptureDone(videoResult);
    assert(this.videoResultFile !== null);
    await this.review.setReviewVideo(this.videoResultFile);
    await this.reviewIntentResult(
        {resolution: videoResult.resolution, duration: videoResult.duration});
    ChromeHelper.getInstance().maybeTriggerSurvey();
  }
}
