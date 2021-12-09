// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from '../../../assert.js';
// eslint-disable-next-line no-unused-vars
import {StreamConstraints} from '../../../device/stream_constraints.js';
import {Point} from '../../../geometry.js';
import {Filenamer} from '../../../models/file_namer.js';
import {ChromeHelper} from '../../../mojo/chrome_helper.js';
import {
  CanceledError,
  Facing,     // eslint-disable-line no-unused-vars
  ImageBlob,  // eslint-disable-line no-unused-vars
  MimeType,
  Resolution,  // eslint-disable-line no-unused-vars
} from '../../../type.js';

import {ModeFactory} from './mode_base.js';
import {
  Photo,
  PhotoHandler,  // eslint-disable-line no-unused-vars
} from './photo.js';

/**
 * Contains photo taking result.
 * @typedef {{
 *     resolution: !Resolution,
 *     blob: !Blob,
 *     mimeType: !MimeType,
 * }}
 */
export let DocumentResult;

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
export class ScanHandler {
  /**
   * Plays UI effect when taking photo.
   * @abstract
   */
  playShutterEffect() {
    assertNotReached();
  }

  /**
   * @param {!ImageBlob} originImage Original photo to be cropped document from.
   * @param {?Array<!Point>} refCorners Initial reference document corner
   *     positions detected by scan API. Sets to null if scan API cannot find
   *     any reference corner from |rawBlob|.
   * @return {!Promise<?{docBlob: !Blob, mimeType: !MimeType}>} Returns the
   *     processed document blob and which mime type user choose to save. Null
   *     for cancel document.
   * @abstract
   */
  async reviewDocument(originImage, refCorners) {
    assertNotReached();
  }

  /**
   * Handles the result document.
   * @param {!DocumentResult} result
   * @param {string} name Name of the document result to be saved as.
   * @return {!Promise}
   * @abstract
   */
  handleResultDocument(result, name) {
    assertNotReached();
  }

  /**
   * @return {!Promise}
   * @abstract
   */
  waitPreviewReady() {
    assertNotReached();
  }
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
     */
    this.handler_ = handler;
  }

  /**
   * @override
   */
  async handleResultPhoto({blob: rawBlob, resolution}) {
    const namer = new Filenamer();
    const helper = await ChromeHelper.getInstance();
    const corners = await helper.scanDocumentCorners(rawBlob);
    const reviewResult = await this.handler_.reviewDocument(
        {blob: rawBlob, resolution}, corners);
    if (reviewResult === null) {
      throw new CanceledError('Cancelled after review document');
    }
    const {docBlob, mimeType} = reviewResult;
    const name = namer.newDocumentName(mimeType);
    let blob = docBlob;
    if (mimeType === MimeType.PDF) {
      blob = await helper.convertToPdf(blob);
    }
    await this.handler_.handleResultDocument(
        {blob, resolution, mimeType}, name);
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

    /**
     * @const {!ScanHandler}
     * @protected
     */
    this.scanHandler_ = handler;
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
