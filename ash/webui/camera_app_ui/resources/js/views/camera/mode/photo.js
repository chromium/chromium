// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from '../../../assert.js';
// eslint-disable-next-line no-unused-vars
import {StreamConstraints} from '../../../device/stream_constraints.js';
import {I18nString} from '../../../i18n_string.js';
import {Filenamer} from '../../../models/file_namer.js';
import * as filesystem from '../../../models/file_system.js';
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
import * as toast from '../../../toast.js';
import {
  CanceledError,
  Facing,  // eslint-disable-line no-unused-vars
  PerfEvent,
  Resolution,
} from '../../../type.js';
import * as util from '../../../util.js';

import {ModeBase, ModeFactory} from './mode_base.js';

/**
 * Contains photo taking result.
 * @typedef {{
 *     resolution: !Resolution,
 *     blob: !Blob,
 *     isVideoSnapshot?: boolean,
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
   * Handles the result photo.
   * @param {!PhotoResult} photo Captured photo result.
   * @param {string} name Name of the photo result to be saved as.
   * @return {!Promise}
   * @abstract
   */
  handleResultPhoto(photo, name) {
    assertNotReached();
  }

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
        new CrosImageCapture(this.stream_.getVideoTracks()[0]);

    /**
     * The observer endpoint for saving metadata.
     * @type {?MojoEndpoint}
     * @protected
     */
    this.metadataObserver_ = null;

    /**
     * Metadata names ready to be saved.
     * @type {!Array<string>}
     * @protected
     */
    this.metadataNames_ = [];
  }

  /**
   * @param {!MediaStream} stream
   */
  updatePreview(stream) {
    this.stream_ = stream;
    this.crosImageCapture_ =
        new CrosImageCapture(this.stream_.getVideoTracks()[0]);
  }

  /**
   * @override
   */
  async start_() {
    const imageName = (new Filenamer()).newImageName();
    if (this.metadataObserver_ !== null) {
      this.metadataNames_.push(Filenamer.getMetadataName(imageName));
    }

    state.set(PerfEvent.PHOTO_CAPTURE_SHUTTER, true);
    try {
      const blob = await this.takePhoto_();
      state.set(PerfEvent.PHOTO_CAPTURE_SHUTTER, false, {facing: this.facing_});
      state.set(PerfEvent.PHOTO_CAPTURE_POST_PROCESSING, true);
      const image = await util.blobToImage(blob);
      const resolution = new Resolution(image.width, image.height);
      await this.handler_.handleResultPhoto({resolution, blob}, imageName);
      state.set(
          PerfEvent.PHOTO_CAPTURE_POST_PROCESSING, false,
          {resolution, facing: this.facing_});
    } catch (e) {
      state.set(PerfEvent.PHOTO_CAPTURE_SHUTTER, false, {hasError: true});
      state.set(
          PerfEvent.PHOTO_CAPTURE_POST_PROCESSING, false, {hasError: true});
      if (!(e instanceof CanceledError)) {
        toast.show(I18nString.ERROR_MSG_TAKE_PHOTO_FAILED);
      }
      throw e;
    }
  }

  /**
   * @private
   * @return {!Promise<!Blob>}
   */
  async takePhoto_() {
    if (state.get(state.State.ENABLE_PTZ)) {
      // Workaround for b/184089334 on PTZ camera to use preview frame as
      // photo result.
      return this.crosImageCapture_.grabJpegFrame();
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
    await this.handler_.waitPreviewReady();
    const results = await this.crosImageCapture_.takePhoto(photoSettings);
    this.handler_.playShutterEffect();
    return results[0];
  }

  /**
   * Adds an observer to save metadata.
   * @return {!Promise} Promise for the operation.
   */
  async addMetadataObserver() {
    if (!this.stream_) {
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
      const parsedMetadata = {};
      for (const entry of metadata.entries) {
        const key = cameraMetadataTagInverseLookup[entry.tag];
        if (key === undefined) {
          // TODO(kaihsien): Add support for vendor tags.
          continue;
        }

        const val = parseMetadata(entry);
        parsedMetadata[key] = val;
      }

      filesystem.saveBlob(
          new Blob(
              [JSON.stringify(parsedMetadata, null, 2)],
              {type: 'application/json'}),
          this.metadataNames_.shift());
    };

    const deviceId = this.stream_.getVideoTracks()[0].getSettings().deviceId;
    this.metadataObserver_ = await deviceOperator.addMetadataObserver(
        deviceId, callback, StreamType.JPEG_OUTPUT);
  }

  /**
   * Removes the observer that saves metadata.
   * @return {!Promise} Promise for the operation.
   */
  async removeMetadataObserver() {
    if (!this.stream_ || this.metadataObserver_ === null) {
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
        this.previewStream_, this.facing_, this.captureResolution_,
        this.handler_);
  }
}
