// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof, assertNotReached} from '../../../assert.js';
// eslint-disable-next-line no-unused-vars
import {StreamConstraints} from '../../../device/stream_constraints.js';
import * as error from '../../../error.js';
import {
  CanceledError,
  ErrorLevel,
  ErrorType,
  Facing,      // eslint-disable-line no-unused-vars
  Resolution,  // eslint-disable-line no-unused-vars
} from '../../../type.js';

/**
 * Base class for controlling capture sequence in different camera modes.
 * @abstract
 */
export class ModeBase {
  /**
   * @param {!MediaStream} stream
   * @param {!Facing} facing
   */
  constructor(stream, facing) {
    /**
     * Stream of current mode.
     * @type {!MediaStream}
     * @protected
     */
    this.stream_ = stream;

    /**
     * Camera facing of current mode.
     * @type {!Facing}
     * @protected
     */
    this.facing_ = facing;

    /**
     * Promise for ongoing capture operation.
     * @type {?Promise}
     * @private
     */
    this.capture_ = null;
  }

  /**
   * Initiates video/photo capture operation.
   * @return {!Promise} Promise for ongoing capture operation.
   */
  startCapture() {
    if (this.capture_ === null) {
      this.capture_ = this.start_().finally(() => this.capture_ = null);
    }
    return this.capture_;
  }

  /**
   * Stops the ongoing capture operation.
   * @return {!Promise} Promise for ongoing capture operation.
   */
  async stopCapture() {
    this.stop_();
    try {
      await this.capture_;
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
   * @return {!Promise} Promise for the operation.
   */
  async addMetadataObserver() {}

  /**
   * Removes the observer that saves metadata.
   * @return {!Promise} Promise for the operation.
   */
  async removeMetadataObserver() {}

  /**
   * Clears everything when mode is not needed anymore.
   * @return {!Promise}
   */
  async clear() {
    await this.stopCapture();
  }

  /**
   * Updates preview stream currently in used.
   * @param {!MediaStream} stream
   * @abstract
   */
  updatePreview(stream) {}

  /**
   * Initiates video/photo capture operation under this mode.
   * @return {!Promise}
   * @protected
   * @abstract
   */
  async start_() {
    assertNotReached();
  }

  /**
   * Stops the ongoing capture operation under this mode.
   * @protected
   */
  stop_() {}
}

/**
 * @abstract
 */
export class ModeFactory {
  /**
   * @param {!StreamConstraints} constraints Constraints for preview
   *     stream.
   * @param {!Resolution} captureResolution
   */
  constructor(constraints, captureResolution) {
    /**
     * Preview stream.
     * @type {?MediaStream}
     * @protected
     */
    this.stream_ = null;

    /**
     * Camera facing of current mode.
     * @type {!Facing}
     * @protected
     */
    this.facing_ = Facing.NOT_SET;

    /**
     * Preview constraints.
     * @type {!StreamConstraints}
     * @protected
     */
    this.constraints_ = constraints;

    /**
     * Capture resolution.
     * @type {!Resolution}
     * @protected
     */
    this.captureResolution_ = captureResolution;
  }

  /**
   * @return {!MediaStream}
   * @protected
   */
  get previewStream_() {
    return assertInstanceof(this.stream_, MediaStream);
  }

  /**
   * @param {!Facing} facing
   */
  setFacing(facing) {
    this.facing_ = facing;
  }

  /**
   * @param {!MediaStream} stream
   */
  setPreviewStream(stream) {
    this.stream_ = stream;
  }

  /**
   * Produces the mode capture object.
   * @return {!ModeBase}
   * @abstract
   */
  produce() {
    assertNotReached();
  }
}
