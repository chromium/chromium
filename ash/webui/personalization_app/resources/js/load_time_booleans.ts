// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Getters for all of the loadTimeData booleans used throughout
 * personalization app.
 * @see //ash/webui/personalization_app/personalization_app_ui.cc
 * Export them as functions so they reload the values when overridden in test.
 */

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';

export function isGooglePhotosIntegrationEnabled() {
  return loadTimeData.getBoolean('isGooglePhotosIntegrationEnabled');
}

export function isGooglePhotosSharedAlbumsEnabled() {
  return loadTimeData.getBoolean('isGooglePhotosSharedAlbumsEnabled');
}

export function isAmbientModeAllowed() {
  return loadTimeData.getBoolean('isAmbientModeAllowed');
}

export function isRgbKeyboardSupported() {
  return loadTimeData.getBoolean('isRgbKeyboardSupported');
}

export function isScreenSaverDurationEnabled() {
  return loadTimeData.getBoolean('isScreenSaverDurationEnabled');
}

export function isScreenSaverPreviewEnabled() {
  return loadTimeData.getBoolean('isScreenSaverPreviewEnabled');
}

export function isPersonalizationJellyEnabled() {
  return loadTimeData.getBoolean('isPersonalizationJellyEnabled');
}

export function isMultiZoneRgbKeyboardSupported() {
  return loadTimeData.getInteger('keyboardBacklightZoneCount') > 1;
}

export function isUserAvatarCustomizationSelectorsEnabled() {
  return loadTimeData.getBoolean('isUserAvatarCustomizationSelectorsEnabled');
}

export function isTimeOfDayScreenSaverEnabled() {
  return loadTimeData.getBoolean('isTimeOfDayScreenSaverEnabled');
}

export function isTimeOfDayWallpaperEnabled() {
  return loadTimeData.getBoolean('isTimeOfDayWallpaperEnabled');
}
