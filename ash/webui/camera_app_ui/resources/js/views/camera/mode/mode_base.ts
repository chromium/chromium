// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from '../../../assert.js';
import {StreamConstraints} from '../../../device/stream_constraints.js';
import * as error from '../../../error.js';
import {
  CanceledError,
  ErrorLevel,
  ErrorType,
  Facing,
  PreviewVideo,
  Resolution,
} from '../../../type.js';

/**
 * Base class for controlling capture sequence in different camera modes.
 */
export abstract class ModeBase {
  /**
   * Promise for ongoing capture operation.
   */
  private capture: Promise<() => Promise<void>>|null = null;

  /**
   * @param video Preview video.
   * @param facing Camera facing of current mode.
   */
  constructor(
      protected video: PreviewVideo, protected readonly facing: Facing) {}

  /**
   * Initiates video/photo capture operation.
   * @return Promise for ongoing capture operation and resolved to handler
   *     function which should be run after capture finished.
   */
  startCapture(): Promise<() => Promise<void>> {
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
   * @return Promise for ongoing capture operation.
   */
  async stopCapture(): Promise<void> {
    this.stop();
    try {
      await this.capture;
    } catch (e) {
      if (e instanceof CanceledError) {
        return;
      }
      error.reportError(
          ErrorType.STOP_CAPTURE_FAILURE, ErrorLevel.ERROR,
          assertInstanceof(e, Error));
    }
  }

  /**
   * Adds an observer to save image metadata.
   * @return Promise for the operation.
   */
  async addMetadataObserver(): Promise<void> {
    // To be overridden by subclass.
  }

  /**
   * Removes the observer that saves metadata.
   * @return Promise for the operation.
   */
  async removeMetadataObserver(): Promise<void> {
    // To be overridden by subclass.
  }

  /**
   * Clears everything when mode is not needed anymore.
   */
  async clear(): Promise<void> {
    await this.stopCapture();
  }

  /**
   * Updates preview video currently in used.
   */
  abstract updatePreview(previewVideo: PreviewVideo): void;

  /**
   * Initiates video/photo capture operation under this mode.
   */
  protected abstract start(): Promise<() => Promise<void>>;

  /**
   * Stops the ongoing capture operation under this mode.
   */
  protected stop(): void {
    // To be overridden by subclass.
  }
}

export abstract class ModeFactory {
  /**
   * Preview video.
   */
  protected previewVideo: PreviewVideo|null = null;

  /**
   * Preview stream.
   */
  protected stream: MediaStream|null = null;

  /**
   * Camera facing of current mode.
   */
  protected facing = Facing.NOT_SET;

  /**
   * @param constraints Constraints for preview stream.
   * @param captureResolution Capture resolution.
   */
  constructor(
      protected readonly constraints: StreamConstraints,
      protected readonly captureResolution: Resolution) {}

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
