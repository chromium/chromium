// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Enumeration of the topic source, i.e. where the photos come from.
 * Values need to stay in sync with the enum |ash::AmbientModeTopicSource|.
 * @enum {number}
 */
/* #export */ const AmbientModeTopicSource = {
  UNKNOWN: -1,
  GOOGLE_PHOTOS: 0,
  ART_GALLERY: 1,
};

/** @enum {string} */
/* #export */ const AmbientModeTemperatureUnit = {
  UNKNOWN: 'unknown',
  FAHRENHEIT: 'fahrenheit',
  CELSIUS: 'celsius',
};

/**
 * Item of AmbientModeTopicSource.
 *
 * @typedef {{
 *   topicSource: AmbientModeTopicSource,
 *   hasGooglePhotosAlbums: boolean,
 * }}
 */
/* #export */ let TopicSourceItem;

/**
 * Album metadata for UI.
 *
 * @typedef {{
 *   topicSource: AmbientModeTopicSource,
 *   albumId: string,
 *   checked: boolean,
 *   description: string,
 *   title: string,
 *   url: string,
 *   recentHighlightsUrls: Array<string>,
 * }}
 */
/* #export */ let AmbientModeAlbum;

/**
 * Settings containing topic source and the albums.
 *
 * @typedef {{
 *   albums: !Array<!AmbientModeAlbum>,
 *   topicSource: !AmbientModeTopicSource,
 * }}
 */
/* #export */ let AmbientModeSettings;
