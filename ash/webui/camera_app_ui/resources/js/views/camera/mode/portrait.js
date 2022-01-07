// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from '../../../assert.js';
// eslint-disable-next-line no-unused-vars
import {StreamConstraints} from '../../../device/stream_constraints.js';
import {I18nString} from '../../../i18n_string.js';
import {CrosImageCapture} from '../../../mojo/image_capture.js';
import {Effect} from '../../../mojo/type.js';
import * as toast from '../../../toast.js';
import {
  Facing,      // eslint-disable-line no-unused-vars
  Metadata,    // eslint-disable-line no-unused-vars
  Resolution,  // eslint-disable-line no-unused-vars
} from '../../../type.js';
import * as util from '../../../util.js';
import {WaitableEvent} from '../../../waitable_event.js';

import {
  Photo,
  PhotoFactory,
  PhotoHandler,  // eslint-disable-line no-unused-vars
} from './photo.js';

/**
 * Contains photo taking result.
 * @typedef {{
 *     timestamp: number,
 *     resolution: !Resolution,
 *     blob: !Blob,
 *     metadata: ?Metadata,
 *     pendingPortrait: !Promise<?{blob: !Blob, metadata: ?Metadata}>,
 * }}
 */
export let PortraitResult;

/**
 * Provides external dependency functions used by portrait mode and handles the
 * captured result photo.
 * @interface
 */
export class PortraitHandler extends PhotoHandler {
  /**
   * @param {!Promise<!PortraitResult>} pendingPortraitResult
   * @return {!Promise<void>}
   * @abstract
   */
  onPortraitCaptureDone(pendingPortraitResult) {
    assertNotReached();
  }
}

/**
 * Portrait mode capture controller.
 */
export class Portrait extends Photo {
  /**
   * @param {!MediaStream} stream
   * @param {!Facing} facing
   * @param {!Resolution} captureResolution
   * @param {!PortraitHandler} handler
   */
  constructor(stream, facing, captureResolution, handler) {
    super(stream, facing, captureResolution, handler);

    /**
     * @const {!PortraitHandler}
     * @private
     */
    this.portraitHandler_ = handler;
  }

  /**
   * @override
   */
  async start() {
    const timestamp = Date.now();
    if (this.crosImageCapture_ === null) {
      this.crosImageCapture_ =
          new CrosImageCapture(this.stream.getVideoTracks()[0]);
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

    let /** !Promise<!Blob> */ reference;
    let /** !Promise<!Blob> */ portrait;
    let /** ?WaitableEvent<!Metadata> */ waitForMetadata = null;
    let /** ?WaitableEvent<!Metadata> */ waitForPortraitMetadata = null;
    if (this.metadataObserver_ !== null) {
      waitForMetadata =
          /** @type{!WaitableEvent<!Metadata>} */ (new WaitableEvent());
      waitForPortraitMetadata =
          /** @type{!WaitableEvent<!Metadata>} */ (new WaitableEvent());
      this.pendingResultForMetadata_.push(
          waitForMetadata, waitForPortraitMetadata);
    }
    try {
      [reference, portrait] = await this.crosImageCapture_.takePhoto(
          photoSettings, [Effect.PORTRAIT_MODE]);
      this.portraitHandler_.playShutterEffect();
    } catch (e) {
      toast.show(I18nString.ERROR_MSG_TAKE_PHOTO_FAILED);
      throw e;
    }

    const pendingPortraitResult = (async () => {
      const blob = await reference;
      const image = await util.blobToImage(blob);
      const resolution = new Resolution(image.width, image.height);
      const metadata = await (waitForMetadata?.wait() ?? null);
      const pendingPortrait = (async () => {
        let /** !Blob */ portraitBlob;
        try {
          portraitBlob = await portrait;
        } catch (e) {
          // Portrait image may failed due to absence of human faces.
          // TODO(inker): Log non-intended error.
          return null;
        }
        const metadata = await (waitForPortraitMetadata?.wait() ?? null);
        return {blob: portraitBlob, metadata};
      })();

      return {timestamp, resolution, blob, metadata, pendingPortrait};
    })();

    return () => this.portraitHandler_.onPortraitCaptureDone(
               pendingPortraitResult);
  }
}

/**
 * Factory for creating portrait mode capture object.
 */
export class PortraitFactory extends PhotoFactory {
  /**
   * @param {!StreamConstraints} constraints Constraints for preview
   *     stream.
   * @param {!Resolution} captureResolution
   * @param {!PortraitHandler} handler
   */
  constructor(constraints, captureResolution, handler) {
    super(constraints, captureResolution, handler);

    /**
     * @const {!PhotoHandler}
     * @protected
     */
    this.portraitHandler_ = handler;
  }

  /**
   * @override
   */
  produce() {
    return new Portrait(
        this.previewStream, this.facing, this.captureResolution,
        this.portraitHandler_);
  }
}
