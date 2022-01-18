// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {StreamConstraints} from '../../../device/stream_constraints.js';
import {I18nString} from '../../../i18n_string.js';
import {CrosImageCapture} from '../../../mojo/image_capture.js';
import {Effect} from '../../../mojo/type.js';
import * as toast from '../../../toast.js';
import {
  Facing,
  Metadata,
  PreviewVideo,
  Resolution,
} from '../../../type.js';
import * as util from '../../../util.js';
import {WaitableEvent} from '../../../waitable_event.js';

import {ModeBase} from './mode_base.js';
import {
  Photo,
  PhotoFactory,
  PhotoHandler,
} from './photo.js';

/**
 * Contains photo taking result.
 */
export interface PortraitResult {
  timestamp: number;
  resolution: Resolution;
  blob: Blob;
  metadata: Metadata|null;
  pendingPortrait: Promise<{blob: Blob, metadata: Metadata|null}>|null;
}

/**
 * Provides external dependency functions used by portrait mode and handles the
 * captured result photo.
 */
export interface PortraitHandler extends PhotoHandler {
  onPortraitCaptureDone(pendingPortraitResult: Promise<PortraitResult>):
      Promise<void>;
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
    if (this.crosImageCapture === null) {
      this.crosImageCapture = new CrosImageCapture(this.video.getVideoTrack());
    }

    let photoSettings: PhotoSettings;
    if (this.captureResolution) {
      photoSettings = {
        imageWidth: this.captureResolution.width,
        imageHeight: this.captureResolution.height,
      };
    } else {
      const caps = await this.crosImageCapture.getPhotoCapabilities();
      photoSettings = {
        imageWidth: caps.imageWidth.max,
        imageHeight: caps.imageHeight.max,
      };
    }

    let reference: Promise<Blob>;
    let portrait: Promise<Blob>;
    let waitForMetadata: WaitableEvent<Metadata>|null = null;
    let waitForPortraitMetadata: WaitableEvent<Metadata>|null = null;
    if (this.metadataObserver !== null) {
      waitForMetadata = new WaitableEvent();
      waitForPortraitMetadata = new WaitableEvent();
      this.pendingResultForMetadata.push(
          waitForMetadata, waitForPortraitMetadata);
    }
    try {
      [reference, portrait] = await this.crosImageCapture.takePhoto(
          photoSettings, [Effect.PORTRAIT_MODE]);
      this.portraitHandler.playShutterEffect();
    } catch (e) {
      toast.show(I18nString.ERROR_MSG_TAKE_PHOTO_FAILED);
      throw e;
    }

    const pendingPortraitResult = (async () => {
      const blob = await reference;
      const image = await util.blobToImage(blob);
      const resolution = new Resolution(image.width, image.height);
      const metadata = await (waitForMetadata?.wait() ?? null);
      const pendingPortrait = (async () => {
        let portraitBlob: Blob;
        try {
          portraitBlob = await portrait;
        } catch (e) {
          // Portrait image may failed due to absence of human faces.
          // TODO(inker): Log non-intended error.
          return null;
        }
        const metadata = await (waitForPortraitMetadata?.wait() ?? null);
        return {blob: portraitBlob, metadata};
      })();

      return {timestamp, resolution, blob, metadata, pendingPortrait};
    })();

    return () => this.portraitHandler.onPortraitCaptureDone(
               pendingPortraitResult);
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
