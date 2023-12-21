// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from './assert.js';
import {reportError} from './error.js';
import {FileAccessEntry} from './models/file_system_access_entry.js';
import {extractImageFromBlob} from './thumbnailer.js';
import {
  ErrorLevel,
  ErrorType,
  MimeType,
} from './type.js';

/**
 * Cover photo of gallery button.
 */
export class CoverPhoto {
  /**
   * @param name File name of cover photo.
   * @param url Url to its cover photo. Might be null if the cover is failed to
   *     load.
   * @param draggable If the file type support share by drag/drop cover photo.
   */
  private constructor(
      readonly name: string,
      readonly url: string|null,
      readonly draggable: boolean,
  ) {}

  /**
   * Releases resources used by this cover photo.
   */
  release(): void {
    if (this.url !== null) {
      URL.revokeObjectURL(this.url);
    }
  }

  /**
   * Creates CoverPhoto objects from photo file.
   */
  static async create(file: FileAccessEntry): Promise<CoverPhoto|null> {
    const blob = await file.file();
    if (blob.size === 0) {
      reportError(
          ErrorType.EMPTY_FILE,
          ErrorLevel.ERROR,
          new Error('The file to generate cover photo is empty'),
      );
      return null;
    }

    try {
      const cover = await extractImageFromBlob(blob);
      const draggable = blob.type !== MimeType.MP4;
      return new CoverPhoto(file.name, URL.createObjectURL(cover), draggable);
    } catch (e) {
      reportError(
          ErrorType.BROKEN_THUMBNAIL, ErrorLevel.ERROR,
          assertInstanceof(e, Error));
      return new CoverPhoto(file.name, null, false);
    }
  }
}
