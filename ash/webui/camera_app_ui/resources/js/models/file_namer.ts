// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from '../assert.js';
import {
  MimeType,
  VideoType,
} from '../type.js';

/**
 * The prefix of image files.
 */
export const IMAGE_PREFIX = 'IMG_';

/**
 * The prefix of video files.
 */
export const VIDEO_PREFIX = 'VID_';

/**
 * The prefix of scanned document files.
 */
export const DOCUMENT_PREFIX = 'SCN_';

/**
 * The suffix of burst image files.
 */
const BURST_SUFFIX = '_BURST';

/**
 * The suffix of cover image for a series of burst image files.
 */
const BURST_COVER_SUFFIX = '_COVER';

/**
 * Transforms from capture timestamp to datetime name.
 *
 * @param timestamp Timestamp to be transformed.
 * @return Transformed datetime name.
 */
function timestampToDatetimeName(timestamp: number): string {
  function pad(n: number) {
    return (n < 10 ? '0' : '') + n;
  }
  const date = new Date(timestamp);
  return date.getFullYear() + pad(date.getMonth() + 1) + pad(date.getDate()) +
      '_' + pad(date.getHours()) + pad(date.getMinutes()) +
      pad(date.getSeconds());
}

const FILE_NAME_PATTERN = (() => {
  const timestampRegex = String.raw`\d{8}_\d{6}`;
  const burstSuffixRegex =
      String.raw`${BURST_SUFFIX}\d{5}(${BURST_COVER_SUFFIX})?`;
  const conflictHandlingRegex = String.raw`( \(\d+\))?`;
  const imageRegex = String.raw`^${IMAGE_PREFIX}${timestampRegex}${
      conflictHandlingRegex}\.jpg$`;
  const burstRegex = String.raw`^${IMAGE_PREFIX}${timestampRegex}${
      burstSuffixRegex}${conflictHandlingRegex}\.jpg$`;
  const videoRegex = String.raw`^${VIDEO_PREFIX}${timestampRegex}${
      conflictHandlingRegex}\.(mp4|gif)$`;
  const docRegex = String.raw`^${DOCUMENT_PREFIX}${timestampRegex}${
      conflictHandlingRegex}\.(jpg|pdf)$`;
  return new RegExp([
    imageRegex,
    burstRegex,
    videoRegex,
    docRegex,
  ].map((r) => `(${r})`).join('|'));
})();

/**
 * Filenamer for single camera session.
 */
export class Filenamer {
  /**
   * Timestamp of camera session.
   */
  private readonly timestamp: number;

  /**
   * Number of already saved burst images.
   */
  private burstCount = 0;

  /**
   * @param timestamp Timestamp of camera session.
   */
  constructor(timestamp?: number) {
    this.timestamp = timestamp ?? Date.now();
  }

  /**
   * Creates new filename for burst image.
   *
   * @param isCover If the image is set as cover of the burst.
   * @return New filename.
   */
  newBurstName(isCover: boolean): string {
    function prependZeros(n: number, width: number) {
      return String(n).padStart(width, '0');
    }
    return IMAGE_PREFIX + timestampToDatetimeName(this.timestamp) +
        BURST_SUFFIX + prependZeros(++this.burstCount, 5) +
        (isCover ? BURST_COVER_SUFFIX : '') + '.jpg';
  }

  /**
   * Creates new filename for video.
   *
   * @return New filename.
   */
  newVideoName(videoType: VideoType): string {
    return VIDEO_PREFIX + timestampToDatetimeName(this.timestamp) + '.' +
        videoType;
  }

  /**
   * Creates new filename for image.
   *
   * @return New filename.
   */
  newImageName(): string {
    return IMAGE_PREFIX + timestampToDatetimeName(this.timestamp) + '.jpg';
  }

  /**
   * Creates new filename for pdf.
   *
   * @return New filename.
   */
  newDocumentName(mimeType: MimeType): string {
    const ext = (() => {
      switch (mimeType) {
        case MimeType.JPEG:
          return 'jpg';
        case MimeType.PDF:
          return 'pdf';
        default:
          assertNotReached(`Unknown type ${mimeType}`);
      }
    })();
    return DOCUMENT_PREFIX + timestampToDatetimeName(this.timestamp) + '.' +
        ext;
  }

  /**
   * Get the metadata name from image name.
   *
   * @param imageName Name of image to derive the metadata name.
   * @return Metadata name of the image.
   */
  static getMetadataName(imageName: string): string {
    return imageName.replace(/\.[^/.]+$/, '.json');
  }

  /**
   * Returns true if the file name matches the format that CCA generates.
   *
   * @param fileName Name of the file.
   * @return True if it matches CCA file naming format.
   */
  static isCCAFileFormat(fileName: string): boolean {
    return FILE_NAME_PATTERN.test(fileName);
  }
}
