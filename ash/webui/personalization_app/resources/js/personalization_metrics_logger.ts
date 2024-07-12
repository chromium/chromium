// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {ColorScheme} from '../color_scheme.mojom-webui.js';
import {StaticColor, TopicSource} from '../personalization_app.mojom-webui.js';

import {GeolocationAccessLevel} from './geolocation_dialog.js';
import {Paths} from './personalization_router_element.js';

// Numerical values are used for metrics; do not change or reuse values. These
// enum values map to Paths enum string values from
// personalization_router_element.ts.
const enum MetricsPath {
  AMBIENT = 0,
  AMBIENT_ALBUMS = 1,
  WALLPAPER_COLLECTION_IMAGES = 2,
  WALLPAPER = 3,
  WALLPAPER_GOOGLE_PHOTO_COLLECTION = 4,
  WALLPAPER_LOCAL_COLLECTION = 5,
  ROOT = 6,
  USER = 7,
  WALLPAPER_SEA_PEN_COLLECTION = 8,
  WALLPAPER_SEA_PEN_RESULTS = 9,
  WALLPAPER_SEA_PEN_FREEFORM = 10,

  MAX_VALUE = WALLPAPER_SEA_PEN_FREEFORM,
}

const enum HistogramName {
  PATH = 'Ash.Personalization.Path',
  AMBIENT_ALBUMS_PATH = 'Ash.Personalization.AmbientMode.AlbumsPath',
  AMBIENT_OPTIN = 'Ash.Personalization.AmbientMode.OptIn',
  AMBIENT_PERFORMANCE_GOOGLE_PHOTOS_PREVIEWS =
      'Ash.Personalization.Ambient.GooglePhotosPreviewsLoadTime',
  AMBIENT_LINK_TO_GOOGLE_PHOTOS_CLICKED =
      'Ash.Personalization.Ambient.LinkToGooglePhotosClicked',
  DYNAMIC_COLOR_COLOR_SCHEME_BUTTON =
      'Ash.Personalization.DynamicColor.ColorSchemeButton',
  DYNAMIC_COLOR_STATIC_COLOR_BUTTON =
      'Ash.Personalization.DynamicColor.StaticColorButton',
  DYNAMIC_COLOR_TOGGLE_BUTTON = 'Ash.Personalization.DynamicColor.ToggleButton',
  KEYBOARD_BACKLIGHT_OPEN_ZONE_CUSTOMIZATION =
      'Ash.Personalization.KeyboardBacklight.OpenZoneCustomization',
  LOCATION_PERMISSION_CHANGE_FROM_DIALOG =
      'ChromeOS.PrivacyHub.Geolocation.AccessLevelChanged.GeolocationDialog',
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
    case Paths.SEA_PEN_COLLECTION:
      return MetricsPath.WALLPAPER_SEA_PEN_COLLECTION;
    case Paths.SEA_PEN_RESULTS:
      return MetricsPath.WALLPAPER_SEA_PEN_RESULTS;
    case Paths.SEA_PEN_FREEFORM:
      return MetricsPath.WALLPAPER_SEA_PEN_FREEFORM;
  }
}

export function logPersonalizationPathUMA(path: Paths) {
  const metricsPath = toMetricsEnum(path);
  assert(metricsPath <= MetricsPath.MAX_VALUE);
  chrome.metricsPrivate.recordEnumerationValue(
      HistogramName.PATH, metricsPath, MetricsPath.MAX_VALUE + 1);
}

export function logAmbientAlbumsPathUMA(topicSource: TopicSource) {
  chrome.metricsPrivate.recordEnumerationValue(
      HistogramName.AMBIENT_ALBUMS_PATH, topicSource,
      TopicSource.MAX_VALUE + 1);
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

export function logKeyboardBacklightOpenZoneCustomizationUMA() {
  chrome.metricsPrivate.recordBoolean(
      HistogramName.KEYBOARD_BACKLIGHT_OPEN_ZONE_CUSTOMIZATION, true);
}

export function logDynamicColorToggleButtonClick(enabled: boolean) {
  chrome.metricsPrivate.recordBoolean(
      HistogramName.DYNAMIC_COLOR_TOGGLE_BUTTON, enabled);
}

export function logDynamicColorStaticColorButtonClick(color: StaticColor) {
  chrome.metricsPrivate.recordEnumerationValue(
      HistogramName.DYNAMIC_COLOR_STATIC_COLOR_BUTTON, color,
      StaticColor.MAX_VALUE);
}

export function logDynamicColorColorSchemeButtonClick(color: ColorScheme) {
  chrome.metricsPrivate.recordEnumerationValue(
      HistogramName.DYNAMIC_COLOR_COLOR_SCHEME_BUTTON, color,
      ColorScheme.MAX_VALUE);
}

export function logAmbientModeLinkToGooglePhotosClick() {
  chrome.metricsPrivate.recordBoolean(
      HistogramName.AMBIENT_LINK_TO_GOOGLE_PHOTOS_CLICKED, true);
}

export function logSystemLocationPermissionChange(
    accessLevel: GeolocationAccessLevel) {
  chrome.metricsPrivate.recordEnumerationValue(
      HistogramName.LOCATION_PERMISSION_CHANGE_FROM_DIALOG, accessLevel,
      GeolocationAccessLevel.MAX_VALUE + 1);
}
