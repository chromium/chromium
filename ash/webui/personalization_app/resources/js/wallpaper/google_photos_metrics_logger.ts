// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

/**
 * This enum is tied directly to a UMA enum defined in
 * //tools/metrics/histograms/enums.xml and should always reflect it (do not
 * change one without changing the other).
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 */
export enum WallpaperGooglePhotosSource {
  PHOTOS = 0,
  ALBUMS = 1,  // This enum will be retired with the shared albums feature.
  OWNED_ALBUMS = 2,
  SHARED_ALBUMS = 3,
  NUM_SOURCES = 4,
}

const WallpaperGooglePhotosSourceHistogramName: string =
    'Ash.Wallpaper.GooglePhotos.Source2';

/**
 * Records the section of the Wallpaper app from which a new Google Photos
 * wallpaper is selected.
 */
export function recordWallpaperGooglePhotosSourceUMA(
    source: WallpaperGooglePhotosSource) {
  assert(source < WallpaperGooglePhotosSource.NUM_SOURCES);

  chrome.metricsPrivate.recordEnumerationValue(
      WallpaperGooglePhotosSourceHistogramName, source,
      WallpaperGooglePhotosSource.NUM_SOURCES);
}
