// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {I18nString} from '../../i18n_string.js';
import {TakePhotoResult} from '../../mojo/image_capture.js';
import {Effect} from '../../mojo/type.js';
import * as toast from '../../toast.js';
import {
  Facing,
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
      captureResolution: Resolution,
      private readonly portraitHandler: PortraitHandler,
  ) {
    super(video, facing, captureResolution, portraitHandler);
  }

  async start(): Promise<() => Promise<void>> {
    const timestamp = Date.now();
    let photoSettings: PhotoSettings;
    if (this.captureResolution) {
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
    try {
      [reference, portrait] = await this.getImageCapture().takePhoto(
          photoSettings, [Effect.PORTRAIT_MODE]);
      this.portraitHandler.playShutterEffect();
    } catch (e) {
      toast.show(I18nString.ERROR_MSG_TAKE_PHOTO_FAILED);
      throw e;
    }

    const toPhotoResult = async (blob, metadata) => {
      const image = await util.blobToImage(blob);
      const resolution = new Resolution(image.width, image.height);
      return {blob, timestamp, resolution, metadata};
    };

    const pendingReference = (async () => {
      const blob = await reference.pendingBlob;
      const metadata = await reference.pendingMetadata;
      return toPhotoResult(blob, metadata);
    })();

    const pendingPortrait = (async () => {
      let blob: Blob;
      try {
        blob = await portrait.pendingBlob;
      } catch (e) {
        // Portrait image may failed due to absence of human faces.
        // TODO(inker): Log non-intended error.
        return null;
      }
      const metadata = await reference.pendingMetadata;
      return toPhotoResult(blob, metadata);
    })();
    return () => this.portraitHandler.onPortraitCaptureDone(
               pendingReference, pendingPortrait);
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
      captureResolution: Resolution,
      protected readonly portraitHandler: PortraitHandler,
  ) {
    super(constraints, captureResolution, portraitHandler);
  }

  produce(): ModeBase {
    return new Portrait(
        this.previewVideo, this.facing, this.captureResolution,
        this.portraitHandler);
  }
}
