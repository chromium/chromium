// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// eslint-disable-next-line no-unused-vars
import {StreamConstraints} from '../../../device/stream_constraints.js';
import {Point} from '../../../geometry.js';
import {
  Facing,      // eslint-disable-line no-unused-vars
  ImageBlob,   // eslint-disable-line no-unused-vars
  Resolution,  // eslint-disable-line no-unused-vars
} from '../../../type.js';

import {ModeFactory} from './mode_base.js';
import {
  Photo,
  PhotoHandler,  // eslint-disable-line no-unused-vars
  PhotoResult,   // eslint-disable-line no-unused-vars
} from './photo.js';

/**
 * @param {!Resolution} size Size of image to be cropped document from.
 * @return {!Array<!Point>}
 */
export function getDefaultScanCorners(size) {
  // No initial guess from scan API, position corners in box of portrait A4
  // size occupied with 80% center area.
  const WIDTH_A4 = 210;
  const HEIGHT_A4 = 297;
  const {width: w, height: h} = size;
  const [width, height] = size.aspectRatio > WIDTH_A4 / HEIGHT_A4 ?
      [h / w * WIDTH_A4 / HEIGHT_A4 * 0.8, 0.8] :
      [0.8, w / h * HEIGHT_A4 / WIDTH_A4 * 0.8];
  return [
    new Point(0.5 - width / 2, 0.5 - height / 2),
    new Point(0.5 - width / 2, 0.5 + height / 2),
    new Point(0.5 + width / 2, 0.5 + height / 2),
    new Point(0.5 + width / 2, 0.5 - height / 2),
  ];
}

/**
 * Provides external dependency functions used by photo mode and handles the
 * captured result photo.
 * @interface
 */
export class ScanHandler extends PhotoHandler {
  /**
   * @param {!Promise<!PhotoResult>} pendingPhotoResult
   * @return {!Promise}
   * @abstract
   */
  async onDocumentCaptureDone(pendingPhotoResult) {}
}

/**
 * @implements {PhotoHandler}
 */
class DocumentPhotoHandler {
  /**
   * @param {!ScanHandler} handler
   */
  constructor(handler) {
    /**
     * @const {!ScanHandler}
     * @private
     */
    this.handler_ = handler;
  }

  /**
   * @override
   */
  playShutterEffect() {
    this.handler_.playShutterEffect();
  }

  /**
   * @override
   */
  waitPreviewReady() {
    return this.handler_.waitPreviewReady();
  }

  /**
   * @override
   */
  onPhotoError() {
    this.handler_.onPhotoError();
  }

  /**
   * @override
   */
  onPhotoCaptureDone(pendingPhotoResult) {
    return this.handler_.onDocumentCaptureDone(pendingPhotoResult);
  }
}

/**
 * Photo mode capture controller.
 */
export class Scan extends Photo {
  /**
   * @param {!MediaStream} stream
   * @param {!Facing} facing
   * @param {!Resolution} captureResolution
   * @param {!ScanHandler} handler
   */
  constructor(stream, facing, captureResolution, handler) {
    super(stream, facing, captureResolution, new DocumentPhotoHandler(handler));
  }
}

/**
 * Factory for creating photo mode capture object.
 */
export class ScanFactory extends ModeFactory {
  /**
   * @param {!StreamConstraints} constraints Constraints for preview
   *     stream.
   * @param {!Resolution} captureResolution
   * @param {!ScanHandler} handler
   */
  constructor(constraints, captureResolution, handler) {
    super(constraints, captureResolution);

    /**
     * @const {!ScanHandler}
     * @protected
     */
    this.handler_ = handler;
  }

  /**
   * @override
   */
  produce() {
    return new Scan(
        this.previewStream_,
        this.facing_,
        this.captureResolution_,
        this.handler_,
    );
  }
}
