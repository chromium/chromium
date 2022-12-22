// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';

import {Paths} from './personalization_router_element.js';

// Numerical values are used for metrics; do not change or reuse values. These
// enum values map to Paths enum string values from
// personalization_router_element.ts.
enum MetricsPath {
  AMBIENT = 0,
  AMBIENT_ALBUMS = 1,
  WALLPAPER_COLLECTION_IMAGES = 2,
  WALLPAPER = 3,
  WALLPAPER_GOOGLE_PHOTO_COLLECTION = 4,
  WALLPAPER_LOCAL_COLLECTION = 5,
  ROOT = 6,
  USER = 7,

  MAX_VALUE = USER,
}

const enum HistogramName {
  PATH = 'Ash.Personalization.Path',
  AMBIENT_OPTIN = 'Ash.Personalization.AmbientMode.OptIn',
  AMBIENT_PERFORMANCE_GOOGLE_PHOTOS_PREVIEWS =
      'Ash.Personalization.Ambient.GooglePhotosPreviewsLoadTime',
}

function toMetricsEnum(path: Paths) {
  switch (path) {
    case Paths.AMBIENT:
      return MetricsPath.AMBIENT;
    case Paths.AMBIENT_ALBUMS:
      return MetricsPath.AMBIENT_ALBUMS;
    case Paths.COLLECTION_IMAGES:
      return MetricsPath.WALLPAPER_COLLECTION_IMAGES;
    case Paths.COLLECTIONS:
      return MetricsPath.WALLPAPER;
    case Paths.GOOGLE_PHOTOS_COLLECTION:
      return MetricsPath.WALLPAPER_GOOGLE_PHOTO_COLLECTION;
    case Paths.LOCAL_COLLECTION:
      return MetricsPath.WALLPAPER_LOCAL_COLLECTION;
    case Paths.ROOT:
      return MetricsPath.ROOT;
    case Paths.USER:
      return MetricsPath.USER;
  }
}

export function logPersonalizationPathUMA(path: Paths) {
  const metricsPath = toMetricsEnum(path);
  assert(metricsPath <= MetricsPath.MAX_VALUE);
  chrome.metricsPrivate.recordEnumerationValue(
      HistogramName.PATH, metricsPath, MetricsPath.MAX_VALUE + 1);
}

export function logAmbientModeOptInUMA() {
  chrome.metricsPrivate.recordBoolean(HistogramName.AMBIENT_OPTIN, true);
}

export function logGooglePhotosPreviewsLoadTime() {
  // Get elapsed time in ms since the page initialized.
  const timeMs = Math.round(performance.now());
  console.debug(
      HistogramName.AMBIENT_PERFORMANCE_GOOGLE_PHOTOS_PREVIEWS, timeMs);
  chrome.metricsPrivate.recordTime(
      HistogramName.AMBIENT_PERFORMANCE_GOOGLE_PHOTOS_PREVIEWS, timeMs);
}
