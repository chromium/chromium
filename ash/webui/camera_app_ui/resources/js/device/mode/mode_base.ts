// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from '../../assert.js';
import * as error from '../../error.js';
import {CrosImageCapture} from '../../mojo/image_capture.js';
import {
  CanceledError,
  ErrorLevel,
  ErrorType,
  Facing,
  PreviewVideo,
  Resolution,
} from '../../type.js';
import {StreamConstraints} from '../stream_constraints.js';

/**
 * Base class for controlling capture sequence in different camera modes.
 */
export abstract class ModeBase {
  /**
   * Promise for ongoing capture operation.
   */
  private capture: Promise<[Promise<void>]>|null = null;

  /**
   * CrosImageCapture object to capture still photos.
   */
  protected crosImageCapture: CrosImageCapture;

  /**
   * @param video Preview video.
   * @param facing Camera facing of current mode.
   */
  constructor(
      protected video: PreviewVideo, protected readonly facing: Facing) {
    this.crosImageCapture = new CrosImageCapture(video.getVideoTrack());
  }

  /**
   * Initiates video/photo capture operation.
   *
   * @return Promise for the ongoing capture operation. The outer promise is
   *     resolved after the camere usage is finished. The inner promise is
   *     resolved after the post processing part are finished.
   */
  startCapture(): Promise<[Promise<void>]> {
    if (this.capture === null) {
      this.capture = (async () => {
        try {
          return await this.start();
        } finally {
          this.capture = null;
        }
      })();
    }
    return this.capture;
  }

  /**
   * Stops the ongoing capture operation.
   */
  async stopCapture(): Promise<void> {
    this.stop();
    try {
      // We're intentionally ignoring the returned [Promise<void>].
      void await this.capture;
    } catch (e) {
      if (e instanceof CanceledError) {
        return;
      }
      error.reportError(
          ErrorType.STOP_CAPTURE_FAILURE, ErrorLevel.ERROR,
          assertInstanceof(e, Error));
    }
  }

  getImageCapture(): CrosImageCapture {
    return this.crosImageCapture;
  }

  /**
   * Adds an observer to save image metadata.
   */
  async addMetadataObserver(): Promise<void> {
    if (this.video.isExpired()) {
      return;
    }
    await this.crosImageCapture.addMetadataObserver();
  }

  /**
   * Removes the observer that saves metadata.
   */
  removeMetadataObserver(): void {
    if (!this.video.isExpired()) {
      return;
    }
    this.crosImageCapture.removeMetadataObserver();
  }

  /**
   * Clears everything when mode is not needed anymore.
   */
  async clear(): Promise<void> {
    await this.stopCapture();
  }

  /**
   * Initiates video/photo capture operation under this mode.
   */
  protected abstract start(): Promise<[Promise<void>]>;

  /**
   * Stops the ongoing capture operation under this mode.
   */
  protected stop(): void {
    // To be overridden by subclass.
  }
}

export abstract class ModeFactory {
  protected previewVideo: PreviewVideo|null = null;

  /**
   * Preview stream.
   */
  protected stream: MediaStream|null = null;

  /**
   * Camera facing of current mode.
   */
  protected facing: Facing|null = null;

  /**
   * @param constraints Constraints for preview stream.
   * @param captureResolution Capture resolution.
   */
  constructor(
      protected readonly constraints: StreamConstraints,
      protected readonly captureResolution: Resolution|null) {}

  setFacing(facing: Facing): void {
    this.facing = facing;
  }

  setPreviewVideo(previewVideo: PreviewVideo): void {
    this.previewVideo = previewVideo;
  }

  /**
   * Produces the mode capture object.
   */
  abstract produce(): ModeBase;
}
