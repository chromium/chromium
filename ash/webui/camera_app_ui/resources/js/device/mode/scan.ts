// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '../../assert.js';
import {Point} from '../../geometry.js';
import {
  Facing,
  PreviewVideo,
  Resolution,
} from '../../type.js';
import {StreamConstraints} from '../stream_constraints.js';

import {ModeBase, ModeFactory} from './mode_base.js';
import {
  Photo,
  PhotoHandler,
  PhotoResult,
} from './photo.js';

/**
 * @param size Size of image to be cropped document from.
 */
export function getDefaultScanCorners(size: Resolution): Point[] {
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
 * Provides external dependency functions used by scan mode and handles the
 * captured result photo.
 */
export interface ScanHandler extends PhotoHandler {
  onDocumentCaptureDone(pendingPhotoResult: Promise<PhotoResult>):
      Promise<void>;
}

class DocumentPhotoHandler implements PhotoHandler {
  constructor(private readonly handler: ScanHandler) {}

  playShutterEffect(): void {
    this.handler.playShutterEffect();
  }

  onPhotoError(): void {
    this.handler.onPhotoError();
  }

  onPhotoCaptureDone(pendingPhotoResult: Promise<PhotoResult>): Promise<void> {
    return this.handler.onDocumentCaptureDone(pendingPhotoResult);
  }

  shouldUsePreviewAsPhoto(): boolean {
    return this.handler.shouldUsePreviewAsPhoto();
  }
}

/**
 * Scan mode capture controller.
 */
export class Scan extends Photo {
  constructor(
      video: PreviewVideo, facing: Facing, captureResolution: Resolution|null,
      scanHandler: ScanHandler) {
    super(
        video, facing, captureResolution,
        new DocumentPhotoHandler(scanHandler));
  }
}

/**
 * Factory for creating scan mode capture object.
 */
export class ScanFactory extends ModeFactory {
  /**
   * @param constraints Constraints for preview stream.
   */
  constructor(
      constraints: StreamConstraints, captureResolution: Resolution|null,
      protected readonly handler: ScanHandler) {
    super(constraints, captureResolution);
  }

  produce(): ModeBase {
    assert(this.previewVideo !== null);
    assert(this.facing !== null);
    return new Scan(
        this.previewVideo,
        this.facing,
        this.captureResolution,
        this.handler,
    );
  }
}
