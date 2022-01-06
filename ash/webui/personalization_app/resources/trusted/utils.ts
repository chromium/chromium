// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility functions to be used in trusted code.
 */

import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';

import {WallpaperImage, WallpaperLayout} from '../trusted/personalization_app.mojom-webui.js';

export function isWallpaperImage(obj: any): obj is WallpaperImage {
  return typeof obj?.assetId === 'bigint';
}

export function isFilePath(obj: any): obj is FilePath {
  return typeof obj?.path === 'string' && obj.path;
}

/**
 * Convert a string layout value to the corresponding enum.
 */
export function getWallpaperLayoutEnum(layout: string): WallpaperLayout {
  switch (layout) {
    case 'FILL':
      return WallpaperLayout.kCenterCropped;
    case 'CENTER':  // fall through
    default:
      return WallpaperLayout.kCenter;
  }
}

/**
 * Checks if argument is a string with non-zero length.
 */
export function isNonEmptyString(maybeString: unknown): maybeString is string {
  return typeof maybeString === 'string' && maybeString.length > 0;
}

/**
 * Wallpaper images sometimes have a resolution suffix appended to the end of
 * the image. This is typically to fetch a high resolution image to show as the
 * user's wallpaper. We do not want the full resolution here, so remove the
 * suffix to get a 512x512 preview.
 * TODO(b/186807814) support different resolution parameters here.
 */
export function removeHighResolutionSuffix(url: string): string {
  return url.replace(/=w\d+$/, '');
}

/**
 * Returns whether the given URL starts with http:// or https://.
 */
export function hasHttpScheme(url: string): boolean {
  return url.startsWith('http://') || url.startsWith('https://');
}
