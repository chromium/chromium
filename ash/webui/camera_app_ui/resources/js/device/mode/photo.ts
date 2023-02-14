// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '../../assert.js';
import * as state from '../../state.js';
import {
  CanceledError,
  Facing,
  Metadata,
  PerfEvent,
  PreviewVideo,
  Resolution,
} from '../../type.js';
import * as util from '../../util.js';
import {WaitableEvent} from '../../waitable_event.js';
import {StreamConstraints} from '../stream_constraints.js';

import {ModeBase, ModeFactory} from './mode_base.js';

/**
 * Contains photo taking result.
 */
export interface PhotoResult {
  resolution: Resolution;
  blob: Blob;
  timestamp: number;
  metadata: Metadata|null;
}

/**
 * Provides external dependency functions used by photo mode and handles the
 * captured result photo.
 */
export interface PhotoHandler {
  /**
   * Plays UI effect when taking photo.
   */
  playShutterEffect(): void;

  /**
   * Called when error happen in the capture process.
   */
  onPhotoError(): void;

  onPhotoCaptureDone(pendingPhotoResult: Promise<PhotoResult>): Promise<void>;
}

/**
 * Photo mode capture controller.
 */
export class Photo extends ModeBase {
  /**
   * @param video Preview video.
   * @param facing Camera facing of current mode.
   * @param captureResolution Capture resolution. May be null on device not
   *     support of setting resolution.
   * @param handler Handler for photo operations.
   */
  constructor(
      video: PreviewVideo, facing: Facing,
      protected readonly captureResolution: Resolution|null,
      protected readonly handler: PhotoHandler) {
    super(video, facing);
  }

  async start(): Promise<[Promise<void>]> {
    const timestamp = Date.now();
    state.set(PerfEvent.PHOTO_CAPTURE_SHUTTER, true);
    const {blob, metadata} = await (async () => {
      let hasError = false;
      try {
        return await this.takePhoto();
      } catch (e) {
        hasError = true;
        this.handler.onPhotoError();
        throw e;
      } finally {
        state.set(
            PerfEvent.PHOTO_CAPTURE_SHUTTER, false,
            hasError ? {hasError} : {facing: this.facing});
      }
    })();

    const pendingPhotoResult = (async () => {
      const image = await util.blobToImage(blob);
      const resolution = new Resolution(image.width, image.height);
      return {resolution, blob, timestamp, metadata};
    })();

    return [this.handler.onPhotoCaptureDone(pendingPhotoResult)];
  }

  private async waitPreviewReady(): Promise<void> {
    // Chrome use muted state on video track representing no frame input
    // returned from preview video for a while and call |takePhoto()| with
    // video track in muted state will fail with |kInvalidStateError| exception.
    // To mitigate chance of hitting this error, here we ensure frame inputs
    // from the preview and checked video muted state before taking photo.
    const track = this.video.getVideoTrack();
    const videoEl = this.video.video;
    const waitFrame = async () => {
      const onReady = new WaitableEvent<boolean>();
      const callbackId = videoEl.requestVideoFrameCallback(() => {
        onReady.signal(true);
      });
      (async () => {
        await this.video.onExpired.wait();
        videoEl.cancelVideoFrameCallback(callbackId);
        onReady.signal(false);
      })();
      const ready = await onReady.wait();
      return ready;
    };
    do {
      if (!await waitFrame()) {
        throw new CanceledError('Preview is closed');
      }
    } while (track.muted);
  }

  private async takePhoto(): Promise<{blob: Blob, metadata: Metadata|null}> {
    if (state.get(state.State.ENABLE_PTZ)) {
      // Workaround for b/184089334 on PTZ camera to use preview frame as
      // photo result.
      const blob = await this.getImageCapture().grabJpegFrame();
      this.handler.playShutterEffect();
      return {
        blob,
        metadata: null,
      };
    }
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
    await this.waitPreviewReady();
    const results = await this.getImageCapture().takePhoto(photoSettings);
    this.handler.playShutterEffect();
    return {
      blob: await results[0].pendingBlob,
      metadata: await results[0].pendingMetadata,
    };
  }
}

/**
 * Factory for creating photo mode capture object.
 */
export class PhotoFactory extends ModeFactory {
  /**
   * @param constraints Constraints for preview stream.
   */
  constructor(
      constraints: StreamConstraints, captureResolution: Resolution|null,
      protected readonly handler: PhotoHandler) {
    super(constraints, captureResolution);
  }

  produce(): ModeBase {
    assert(this.previewVideo !== null);
    assert(this.facing !== null);
    return new Photo(
        this.previewVideo, this.facing, this.captureResolution, this.handler);
  }
}
