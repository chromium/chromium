// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility functions to be used for ambient mode.
 */

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {AmbientTheme, TopicSource} from '../../personalization_app.mojom-webui.js';

/**
 * Returns photo count string.
 */
export function getPhotoCount(photoCount: number): string {
  if (photoCount <= 1) {
    return loadTimeData.getStringF(
        'ambientModeAlbumsSubpagePhotosNumSingularDesc', photoCount);
  }
  return loadTimeData.getStringF(
      'ambientModeAlbumsSubpagePhotosNumPluralDesc', photoCount);
}

/**
 * Returns the topic source name.
 */
export function getTopicSourceName(topicSource: TopicSource): string {
  switch (topicSource) {
    case TopicSource.kGooglePhotos:
      return loadTimeData.getString('ambientModeTopicSourceGooglePhotos');
    case TopicSource.kArtGallery:
      return loadTimeData.getString('ambientModeTopicSourceArtGallery');
    case TopicSource.kVideo:
      return loadTimeData.getString('ambientModeTopicSourceVideo');
    default:
      console.warn('Invalid TopicSource value.');
      return '';
  }
}

/**
 * Returns whether the params represent a valid ambient state.
 *
 * Note: TopicSource.kVideo is exclusively paired with AmbientTheme.kVideo
 */
export function isValidTopicSourceAndTheme(
    topicSource: TopicSource, ambientTheme: AmbientTheme|null) {
  return (ambientTheme === AmbientTheme.kVideo &&
          topicSource === TopicSource.kVideo) ||
      (ambientTheme !== AmbientTheme.kVideo &&
       topicSource !== TopicSource.kVideo);
}
