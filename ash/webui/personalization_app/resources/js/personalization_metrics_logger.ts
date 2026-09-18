// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GeolocationAccessLevel} from './geolocation_dialog.js';

const enum HistogramName {
  AMBIENT_PERFORMANCE_GOOGLE_PHOTOS_PREVIEWS =
      'Ash.Personalization.Ambient.GooglePhotosPreviewsLoadTime',
  LOCATION_PERMISSION_CHANGE_FROM_DIALOG =
      'ChromeOS.PrivacyHub.Geolocation.AccessLevelChanged.GeolocationDialog',
}

export function logGooglePhotosPreviewsLoadTime() {
  // Get elapsed time in ms since the page initialized.
  const timeMs = Math.round(performance.now());
  console.debug(
      HistogramName.AMBIENT_PERFORMANCE_GOOGLE_PHOTOS_PREVIEWS, timeMs);
  chrome.metricsPrivate.recordTime(
      HistogramName.AMBIENT_PERFORMANCE_GOOGLE_PHOTOS_PREVIEWS, timeMs);
}

export function logSystemLocationPermissionChange(
    accessLevel: GeolocationAccessLevel) {
  chrome.metricsPrivate.recordEnumerationValue(
      HistogramName.LOCATION_PERMISSION_CHANGE_FROM_DIALOG, accessLevel,
      GeolocationAccessLevel.MAX_VALUE + 1);
}
