// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from '../assert.js';
import {
  MimeType,
  VideoType,  // eslint-disable-line no-unused-vars
} from '../type.js';

/**
 * The prefix of image files.
 * @type {string}
 */
export const IMAGE_PREFIX = 'IMG_';

/**
 * The prefix of video files.
 * @type {string}
 */
export const VIDEO_PREFIX = 'VID_';

/**
 * The prefix of scanned document files.
 * @type {string}
 */
export const DOCUMENT_PREFIX = 'SCN_';

/**
 * The suffix of burst image files.
 * @type {string}
 */
const BURST_SUFFIX = '_BURST';

/**
 * The suffix of cover image for a series of burst image files.
 * @type {string}
 */
const BURST_COVER_SUFFIX = '_COVER';

/**
 * Transforms from capture timestamp to datetime name.
 * @param {number} timestamp Timestamp to be transformed.
 * @return {string} Transformed datetime name.
 */
function timestampToDatetimeName(timestamp) {
  const pad = (n) => (n < 10 ? '0' : '') + n;
  const date = new Date(timestamp);
  return date.getFullYear() + pad(date.getMonth() + 1) + pad(date.getDate()) +
      '_' + pad(date.getHours()) + pad(date.getMinutes()) +
      pad(date.getSeconds());
}

/**
 * Filenamer for single camera session.
 */
export class Filenamer {
  /**
   * @param {number=} timestamp Timestamp of camera session.
   */
  constructor(timestamp) {
    /**
     * Timestamp of camera session.
     * @type {number}
     * @private
     */
    this.timestamp_ = timestamp === undefined ? Date.now() : timestamp;

    /**
     * Number of already saved burst images.
     * @type {number}
     * @private
     */
    this.burstCount_ = 0;
  }

  /**
   * Creates new filename for burst image.
   * @param {boolean} isCover If the image is set as cover of the burst.
   * @return {string} New filename.
   */
  newBurstName(isCover) {
    const prependZeros = (n, width) => {
      n = n + '';
      return new Array(Math.max(0, width - n.length) + 1).join('0') + n;
    };
    return IMAGE_PREFIX + timestampToDatetimeName(this.timestamp_) +
        BURST_SUFFIX + prependZeros(++this.burstCount_, 5) +
        (isCover ? BURST_COVER_SUFFIX : '') + '.jpg';
  }

  /**
   * Creates new filename for video.
   * @param {VideoType} videoType
   * @return {string} New filename.
   */
  newVideoName(videoType) {
    return VIDEO_PREFIX + timestampToDatetimeName(this.timestamp_) + '.' +
        videoType;
  }

  /**
   * Creates new filename for image.
   * @return {string} New filename.
   */
  newImageName() {
    return IMAGE_PREFIX + timestampToDatetimeName(this.timestamp_) + '.jpg';
  }

  /**
   * Creates new filename for pdf.
   * @param {!MimeType} type
   * @return {string} New filename.
   */
  newDocumentName(type) {
    const ext = (() => {
      switch (type) {
        case MimeType.JPEG:
          return 'jpg';
        case MimeType.PDF:
          return 'pdf';
      }
      assertNotReached(`Unknown type ${type}`);
    })();
    return DOCUMENT_PREFIX + timestampToDatetimeName(this.timestamp_) + '.' +
        ext;
  }

  /**
   * Get the metadata name from image name.
   * @param {string} imageName Name of image to derive the metadata name.
   * @return {string} Metadata name of the image.
   */
  static getMetadataName(imageName) {
    return imageName.replace(/\.[^/.]+$/, '.json');
  }
}
