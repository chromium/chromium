// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from '../../../assert.js';
// eslint-disable-next-line no-unused-vars
import {StreamConstraints} from '../../../device/stream_constraints.js';
import {DeviceOperator, parseMetadata} from '../../../mojo/device_operator.js';
import {CrosImageCapture} from '../../../mojo/image_capture.js';
import {
  CameraMetadataTag,
  StreamType,
} from '../../../mojo/type.js';
import {
  closeEndpoint,
  MojoEndpoint,  // eslint-disable-line no-unused-vars
} from '../../../mojo/util.js';
import * as state from '../../../state.js';
import {
  Facing,    // eslint-disable-line no-unused-vars
  Metadata,  // eslint-disable-line no-unused-vars
  PerfEvent,
  Resolution,
} from '../../../type.js';
import * as util from '../../../util.js';
import {WaitableEvent} from '../../../waitable_event.js';

import {ModeBase, ModeFactory} from './mode_base.js';

/**
 * Contains photo taking result.
 * @typedef {{
 *     resolution: !Resolution,
 *     blob: !Blob,
 *     timestamp: number,
 *     metadata: ?Metadata,
 * }}
 */
export let PhotoResult;

/**
 * Provides external dependency functions used by photo mode and handles the
 * captured result photo.
 * @interface
 */
export class PhotoHandler {
  /**
   * Plays UI effect when taking photo.
   */
  playShutterEffect() {}

  /**
   * @return {!Promise}
   * @abstract
   */
  waitPreviewReady() {
    assertNotReached();
  }

  /**
   * Called when error happen in the capture process.
   * @abstract
   */
  onPhotoError() {
    assertNotReached();
  }

  /**
   * @param {!Promise<!PhotoResult>} pendingPhotoResult
   * @return {!Promise<void>}
   * @abstract
   */
  onPhotoCaptureDone(pendingPhotoResult) {
    assertNotReached();
  }
}

/**
 * Photo mode capture controller.
 */
export class Photo extends ModeBase {
  /**
   * @param {!MediaStream} stream
   * @param {!Facing} facing
   * @param {!Resolution} captureResolution
   * @param {!PhotoHandler} handler
   */
  constructor(stream, facing, captureResolution, handler) {
    super(stream, facing);

    /**
     * Capture resolution. May be null on device not support of setting
     * resolution.
     * @type {!Resolution}
     * @protected
     */
    this.captureResolution_ = captureResolution;

    /**
     * @const {!PhotoHandler}
     * @protected
     */
    this.handler_ = handler;

    /**
     * CrosImageCapture object to capture still photos.
     * @type {!CrosImageCapture}
     * @protected
     */
    this.crosImageCapture_ =
        new CrosImageCapture(this.stream.getVideoTracks()[0]);

    /**
     * The observer endpoint for saving metadata.
     * @type {?MojoEndpoint}
     * @protected
     */
    this.metadataObserver_ = null;

    /**
     * Pending |PhotoResult| waiting for arrival of their corresponding
     * metadata..
     * @type {!Array<!WaitableEvent<!Metadata>>}
     * @protected
     */
    this.pendingResultForMetadata_ = [];
  }

  /**
   * @param {!MediaStream} stream
   */
  updatePreview(stream) {
    this.stream = stream;
    this.crosImageCapture_ =
        new CrosImageCapture(this.stream.getVideoTracks()[0]);
  }

  /**
   * @override
   */
  async start() {
    const timestamp = Date.now();
    state.set(PerfEvent.PHOTO_CAPTURE_SHUTTER, true);
    const {blob, pendingMetadata} = await (async () => {
      let hasError = false;
      try {
        return await this.takePhoto_();
      } catch (e) {
        hasError = true;
        this.handler_.onPhotoError();
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

    return async () => this.handler_.onPhotoCaptureDone(pendingPhotoResult);
  }

  /**
   * @private
   * @return {!Promise<{blob: !Blob, pendingMetadata: ?Promise<!Metadata>}>}
   */
  async takePhoto_() {
    if (state.get(state.State.ENABLE_PTZ)) {
      // Workaround for b/184089334 on PTZ camera to use preview frame as
      // photo result.
      return {
        blob: await this.crosImageCapture_.grabJpegFrame(),
        pendingMetadata: null,
      };
    }
    let photoSettings;
    if (this.captureResolution_) {
      photoSettings = /** @type {!PhotoSettings} */ ({
        imageWidth: this.captureResolution_.width,
        imageHeight: this.captureResolution_.height,
      });
    } else {
      const caps = await this.crosImageCapture_.getPhotoCapabilities();
      photoSettings = /** @type {!PhotoSettings} */ ({
        imageWidth: caps.imageWidth.max,
        imageHeight: caps.imageHeight.max,
      });
    }

    let /** ?WaitableEvent<!Metadata> */ waitForMetadata = null;
    if (this.metadataObserver_ !== null) {
      waitForMetadata =
          /** @type{!WaitableEvent<!Metadata>} */ (new WaitableEvent());
      this.pendingResultForMetadata_.push(waitForMetadata);
    }
    await this.handler_.waitPreviewReady();
    const results = await this.crosImageCapture_.takePhoto(photoSettings);
    this.handler_.playShutterEffect();
    return {
      blob: await results[0],
      pendingMetadata: waitForMetadata?.wait() ?? null,
    };
  }

  /**
   * Adds an observer to save metadata.
   * @return {!Promise} Promise for the operation.
   */
  async addMetadataObserver() {
    if (!this.stream) {
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

      this.pendingResultForMetadata_.shift()?.signal(parsedMetadata);
    };

    const deviceId = this.stream.getVideoTracks()[0].getSettings().deviceId;
    this.metadataObserver_ = await deviceOperator.addMetadataObserver(
        deviceId, callback, StreamType.JPEG_OUTPUT);
  }

  /**
   * Removes the observer that saves metadata.
   * @return {!Promise} Promise for the operation.
   */
  async removeMetadataObserver() {
    if (!this.stream || this.metadataObserver_ === null) {
      return;
    }

    const deviceOperator = await DeviceOperator.getInstance();
    if (!deviceOperator) {
      return;
    }

    closeEndpoint(this.metadataObserver_);
    this.metadataObserver_ = null;
  }
}

/**
 * Factory for creating photo mode capture object.
 */
export class PhotoFactory extends ModeFactory {
  /**
   * @param {!StreamConstraints} constraints Constraints for preview
   *     stream.
   * @param {!Resolution} captureResolution
   * @param {!PhotoHandler} handler
   */
  constructor(constraints, captureResolution, handler) {
    super(constraints, captureResolution);

    /**
     * @const {!PhotoHandler}
     * @protected
     */
    this.handler_ = handler;
  }

  /**
   * @override
   */
  produce() {
    return new Photo(
        this.previewStream, this.facing, this.captureResolution, this.handler_);
  }
}
