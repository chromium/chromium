// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '../../assert.js';
import {
  Facing,
  PreviewVideo,
  Resolution,
} from '../../type.js';
import * as util from '../../util.js';

import {ModeBase} from './mode_base.js';
import {
  Photo,
  PhotoFactory,
  PhotoHandler,
  PhotoResult,
} from './photo.js';

/**
 * Crops out maximum possible centered square from the image blob.
 *
 * @return Promise with result cropped square image.
 */
async function cropSquare(blob: Blob): Promise<Blob> {
  const img = await util.blobToImage(blob);
  try {
    const side = Math.min(img.width, img.height);
    const {canvas, ctx} = util.newDrawingCanvas({width: side, height: side});
    ctx.drawImage(
        img, Math.floor((img.width - side) / 2),
        Math.floor((img.height - side) / 2), side, side, 0, 0, side, side);
    // TODO(b/174190121): Patch important exif entries from input blob to
    // result blob.
    const croppedBlob = await util.canvasToJpegBlob(canvas);
    return croppedBlob;
  } finally {
    URL.revokeObjectURL(img.src);
  }
}

/**
 * Cuts the returned photo into square and passed to underlying PhotoHandler.
 */
class SquarePhotoHandler implements PhotoHandler {
  constructor(private readonly handler: PhotoHandler) {}

  playShutterEffect(): void {
    this.handler.playShutterEffect();
  }

  onPhotoError(): void {
    this.handler.onPhotoError();
  }

  async onPhotoCaptureDone(pendingPhotoResult: Promise<PhotoResult>):
      Promise<void> {
    const pendingSquarePhotoResult = (async () => {
      const photoResult = await pendingPhotoResult;
      const croppedBlob = await cropSquare(photoResult.blob);
      return {
        ...photoResult,
        blob: croppedBlob,
      };
    })();
    await this.handler.onPhotoCaptureDone(pendingSquarePhotoResult);
  }
}

/**
 * Square mode capture controller.
 */
export class Square extends Photo {
  constructor(
      video: PreviewVideo,
      facing: Facing,
      captureResolution: Resolution|null,
      handler: PhotoHandler,
  ) {
    super(video, facing, captureResolution, new SquarePhotoHandler(handler));
  }
}

/**
 * Factory for creating square mode capture object.
 */
export class SquareFactory extends PhotoFactory {
  override produce(): ModeBase {
    assert(this.previewVideo !== null);
    assert(this.facing !== null);
    return new Square(
        this.previewVideo, this.facing, this.captureResolution, this.handler);
  }
}
