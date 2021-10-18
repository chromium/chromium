// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// eslint-disable-next-line no-unused-vars
import {StreamConstraints} from '../../../device/stream_constraints.js';
import {Filenamer} from '../../../models/file_namer.js';
import {ChromeHelper} from '../../../mojo/chrome_helper.js';
import {
  CanceledError,
  Facing,  // eslint-disable-line no-unused-vars
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
 * Provides external dependency functions used by photo mode and handles the
 * captured result photo.
 * @interface
 */
export class ScanHandler {
  /**
   * Plays UI effect shutter effect blocking all UI operation.
   */
  playBlockingShutterEffect() {}

  /**
   * @param {!Blob} blob Jpeg Blob as scanned document.
   * @return {!Promise<?MimeType>} Returns which mime type user choose to save.
   *     Null for cancel document.
   */
  async reviewDocument(blob) {}

  /**
   * Handles case when no document detected in photo result.
   */
  handleNoDocument() {}

  /**
   * Handles the result document.
   * @param {!DocumentResult} result
   * @param {string} name Name of the document result to be saved as.
   * @return {!Promise}
   * @abstract
   */
  handleResultDocument(result, name) {}

  /**
   * Handles when cancel the capture for document.
   * @param {{resolution: !Resolution}} result
   * @abstract
   */
  handleCancelDocument(result) {}

  /**
   * @return {!Promise}
   * @abstract
   */
  waitPreviewReady() {}
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
    if (corners.length === 0) {
      this.handler_.handleNoDocument();
      throw new CanceledError(`Couldn't detect a document`);
    }
    const jpegBlob =
        await helper.convertToDocument(rawBlob, corners, MimeType.JPEG);
    const mimeType = await this.handler_.reviewDocument(jpegBlob);
    if (mimeType === null) {
      this.handler_.handleCancelDocument({resolution});
      return;
    }
    const name = namer.newDocumentName(mimeType);
    let blob = jpegBlob;
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
    this.handler_.playBlockingShutterEffect();
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
   * @param {?Resolution} captureResolution
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
   * @param {?Resolution} captureResolution
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
