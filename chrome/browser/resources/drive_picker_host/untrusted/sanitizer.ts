// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

/**
 * Supported file types from the Google Picker.
 */
export enum DriveFileType {
  DOCUMENT = 'document',
  PHOTO = 'photo',
  VIDEO = 'video',
}

/**
 * Sanitized file metadata. Matches the DriveFile struct in Mojom.
 */
export interface SanitizedDriveFile {
  id: string;
  mimeType: string;
  name: string;
  type: string;
  sizeBytes: bigint;
  resourceKey: string|null;
  thumbnailUrl: Url|null;
}

/**
 * Potential errors during sanitization. Matches DrivePickerError in Mojom.
 */
export enum SanitizationError {
  INVALID_FILE_ID = 0,
  INVALID_FILE_METADATA = 1,
  UNSUPPORTED_FILE_TYPE = 2,
  INVALID_FILE_SIZE = 3,
}

/**
 * Logic for sanitizing 3P Google Picker responses.
 */
export class DrivePickerSanitizer {
  /**
   * Sanitizes a single document object from the picker response.
   * Returns the SanitizedDriveFile if valid, otherwise throws a
   * SanitizationError.
   */
  static sanitizeDocument(
      doc: Record<string, unknown>, pickerKeys: {
        ID: string,
        MIME_TYPE: string,
        NAME: string,
        TYPE: string,
        SIZE_BYTES: string,
        RESOURCE_KEY: string,
        THUMBNAIL_URL: string,
      },
      allowedTypes: Set<string>): SanitizedDriveFile {
    const id = doc[pickerKeys.ID];
    const mimeType = doc[pickerKeys.MIME_TYPE];
    const name = doc[pickerKeys.NAME];
    const type = doc[pickerKeys.TYPE];
    const sizeBytes = doc[pickerKeys.SIZE_BYTES];
    const resourceKey = doc[pickerKeys.RESOURCE_KEY];
    const thumbnailUrl = doc[pickerKeys.THUMBNAIL_URL];

    if (!this.isValidId(id)) {
      throw new Error(String(SanitizationError.INVALID_FILE_ID));
    }

    if (typeof mimeType !== 'string' || typeof name !== 'string') {
      throw new Error(String(SanitizationError.INVALID_FILE_METADATA));
    }

    if (typeof type !== 'string' || !allowedTypes.has(type)) {
      throw new Error(String(SanitizationError.UNSUPPORTED_FILE_TYPE));
    }

    if ((typeof sizeBytes !== 'number' && typeof sizeBytes !== 'string') ||
        Number(sizeBytes) < 0) {
      throw new Error(String(SanitizationError.INVALID_FILE_SIZE));
    }

    let sizeBytesBigInt: bigint;
    try {
      sizeBytesBigInt = BigInt(sizeBytes);
    } catch (e) {
      throw new Error(String(SanitizationError.INVALID_FILE_SIZE));
    }

    return {
      id,
      mimeType,
      name,
      type,
      sizeBytes: sizeBytesBigInt,
      resourceKey: this.isValidId(resourceKey) ? resourceKey : null,
      thumbnailUrl: type === DriveFileType.PHOTO ?
          this.sanitizeThumbnailUrl(thumbnailUrl) :
          null,
    };
  }

  /**
   * Validates that an ID or Resource Key contains only safe characters.
   */
  static isValidId(value: unknown): value is string {
    const idRegex = /^[a-zA-Z0-9\-_]+$/;
    return typeof value === 'string' && idRegex.test(value);
  }

  /**
   * Validates that a thumbnail URL points to a trusted Google domain.
   */
  static sanitizeThumbnailUrl(url: unknown): Url|null {
    if (typeof url !== 'string') {
      return null;
    }

    // Matches https://lh[3-6].googleusercontent.com/drive-storage/[safe_chars]
    const urlRegex =
        /^https:\/\/lh[3-6]\.googleusercontent\.com\/drive-storage\/[a-zA-Z0-9\-_=]+$/;
    return urlRegex.test(url) ? url : null;
  }
}
