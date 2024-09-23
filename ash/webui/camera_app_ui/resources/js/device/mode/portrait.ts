// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '../../assert.js';
import {I18nString} from '../../i18n_string.js';
import {TakePhotoResult} from '../../mojo/image_capture.js';
import {Effect} from '../../mojo/type.js';
import {PerfLogger} from '../../perf.js';
import * as toast from '../../toast.js';
import {
  Facing,
  PerfEvent,
  PreviewVideo,
  Resolution,
} from '../../type.js';
import * as util from '../../util.js';
import {StreamConstraints} from '../stream_constraints.js';

import {ModeBase} from './mode_base.js';
import {
  Photo,
  PhotoFactory,
  PhotoHandler,
  PhotoResult,
} from './photo.js';

/**
 * Provides external dependency functions used by portrait mode and handles the
 * captured result photo.
 */
export interface PortraitHandler extends PhotoHandler {
  onPortraitCaptureDone(
      pendingReference: Promise<PhotoResult>,
      pendingPortrait: Promise<PhotoResult|null>): Promise<void>;
}

/**
 * Portrait mode capture controller.
 */
export class Portrait extends Photo {
  constructor(
      video: PreviewVideo,
      facing: Facing,
      captureResolution: Resolution|null,
      private readonly portraitHandler: PortraitHandler,
  ) {
    super(video, facing, captureResolution, portraitHandler);
  }

  override async start(): Promise<[Promise<void>]> {
    const timestamp = Date.now();
    const perfLogger = PerfLogger.getInstance();
    perfLogger.start(PerfEvent.PHOTO_CAPTURE_SHUTTER);
    let photoSettings: PhotoSettings;
    if (this.captureResolution !== null) {
      photoSettings = {
        imageWidth: this.captureResolution.width,
        imageHeight: this.captureResolution.height,
      };
    } else {
      const caps = await this.getImageCapture().getPhotoCapabilities();
      photoSettings = {
        imageWidth: caps.imageWidth.max,
        imageHeight: caps.imageHeight.max,
      };
    }

    let reference: TakePhotoResult;
    let portrait: TakePhotoResult;
    let hasError = false;
    try {
      [reference, portrait] = await this.getImageCapture().takePhoto(
          photoSettings, [Effect.kPortraitMode]);
      this.portraitHandler.playShutterEffect();
    } catch (e) {
      hasError = true;
      toast.show(I18nString.ERROR_MSG_TAKE_PHOTO_FAILED);
      throw e;
    } finally {
      perfLogger.stop(
          PerfEvent.PHOTO_CAPTURE_SHUTTER, {hasError, facing: this.facing});
    }

    async function toPhotoResult(pendingResult: TakePhotoResult) {
      const blob = await pendingResult.pendingBlob;
      const image = await util.blobToImage(blob);
      const resolution = new Resolution(image.width, image.height);
      const metadata = await pendingResult.pendingMetadata;
      return {blob, timestamp, resolution, metadata};
    }
    return [this.portraitHandler.onPortraitCaptureDone(
        toPhotoResult(reference), toPhotoResult(portrait))];
  }
}

/**
 * Factory for creating portrait mode capture object.
 */
export class PortraitFactory extends PhotoFactory {
  /**
   * @param constraints Constraints for preview stream.
   */
  constructor(
      constraints: StreamConstraints,
      captureResolution: Resolution|null,
      protected readonly portraitHandler: PortraitHandler,
  ) {
    super(constraints, captureResolution, portraitHandler);
  }

  override produce(): ModeBase {
    assert(this.previewVideo !== null);
    assert(this.facing !== null);
    return new Portrait(
        this.previewVideo, this.facing, this.captureResolution,
        this.portraitHandler);
  }
}
