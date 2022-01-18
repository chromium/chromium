// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {StreamConstraints} from '../../../device/stream_constraints.js';
import {DeviceOperator, parseMetadata} from '../../../mojo/device_operator.js';
import {CrosImageCapture} from '../../../mojo/image_capture.js';
import {
  CameraMetadataTag,
  StreamType,
} from '../../../mojo/type.js';
import {
  closeEndpoint,
  MojoEndpoint,
} from '../../../mojo/util.js';
import * as state from '../../../state.js';
import {
  CanceledError,
  Facing,
  Metadata,
  PerfEvent,
  PreviewVideo,
  Resolution,
} from '../../../type.js';
import * as util from '../../../util.js';
import {WaitableEvent} from '../../../waitable_event.js';

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
   * CrosImageCapture object to capture still photos.
   */
  protected crosImageCapture: CrosImageCapture;

  /**
   * The observer endpoint for saving metadata.
   */
  protected metadataObserver: MojoEndpoint|null = null;

  /**
   * Pending |PhotoResult| waiting for arrival of their corresponding
   * metadata.
   */
  protected pendingResultForMetadata: Array<WaitableEvent<Metadata>> = [];

  /**
   * @param captureResolution Capture resolution. May be null on device not
   *     support of setting resolution.
   */
  constructor(
      video: PreviewVideo, facing: Facing,
      protected readonly captureResolution: Resolution,
      protected readonly handler: PhotoHandler) {
    super(video, facing);

    this.crosImageCapture = new CrosImageCapture(this.video.getVideoTrack());
  }

  updatePreview(previewVideo: PreviewVideo): void {
    this.video = previewVideo;
    this.crosImageCapture = new CrosImageCapture(this.video.getVideoTrack());
  }

  async start(): Promise<() => Promise<void>> {
    const timestamp = Date.now();
    state.set(PerfEvent.PHOTO_CAPTURE_SHUTTER, true);
    const {blob, pendingMetadata} = await (async () => {
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
      return {resolution, blob, timestamp, metadata: await pendingMetadata};
    })();

    return async () => this.handler.onPhotoCaptureDone(pendingPhotoResult);
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
        await this.video.onExpired;
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

  private async takePhoto():
      Promise<{blob: Blob, pendingMetadata: Promise<Metadata>|null}> {
    if (state.get(state.State.ENABLE_PTZ)) {
      // Workaround for b/184089334 on PTZ camera to use preview frame as
      // photo result.
      return {
        blob: await this.crosImageCapture.grabJpegFrame(),
        pendingMetadata: null,
      };
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

    let waitForMetadata: WaitableEvent<Metadata>|null = null;
    if (this.metadataObserver !== null) {
      waitForMetadata = new WaitableEvent();
      this.pendingResultForMetadata.push(waitForMetadata);
    }
    await this.waitPreviewReady();
    const results = await this.crosImageCapture.takePhoto(photoSettings);
    this.handler.playShutterEffect();
    return {
      blob: await results[0],
      pendingMetadata: waitForMetadata?.wait() ?? null,
    };
  }

  /**
   * Adds an observer to save metadata.
   * @return Promise for the operation.
   */
  async addMetadataObserver(): Promise<void> {
    if (this.video.isExpired()) {
      return;
    }

    const deviceOperator = await DeviceOperator.getInstance();
    if (!deviceOperator) {
      return;
    }

    const cameraMetadataTagInverseLookup = {};
    Object.entries(CameraMetadataTag).forEach(([key, value]) => {
      if (key === 'MIN_VALUE' || key === 'MAX_VALUE') {
        return;
      }
      cameraMetadataTagInverseLookup[value] = key;
    });

    const callback = (metadata) => {
      const parsedMetadata = /** @type {!Record<string, unknown>} */ ({});
      for (const entry of metadata.entries) {
        const key = cameraMetadataTagInverseLookup[entry.tag];
        if (key === undefined) {
          // TODO(kaihsien): Add support for vendor tags.
          continue;
        }

        const val = parseMetadata(entry);
        parsedMetadata[key] = val;
      }

      this.pendingResultForMetadata.shift()?.signal(parsedMetadata);
    };

    const {deviceId} = this.video.getVideoSettings();
    this.metadataObserver = await deviceOperator.addMetadataObserver(
        deviceId, callback, StreamType.JPEG_OUTPUT);
  }

  /**
   * Removes the observer that saves metadata.
   * @return Promise for the operation.
   */
  async removeMetadataObserver(): Promise<void> {
    if (!this.video.isExpired || this.metadataObserver === null) {
      return;
    }

    const deviceOperator = await DeviceOperator.getInstance();
    if (!deviceOperator) {
      return;
    }

    closeEndpoint(this.metadataObserver);
    this.metadataObserver = null;
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
      constraints: StreamConstraints, captureResolution: Resolution,
      protected readonly handler: PhotoHandler) {
    super(constraints, captureResolution);
  }

  produce(): ModeBase {
    return new Photo(
        this.previewVideo, this.facing, this.captureResolution, this.handler);
  }
}
