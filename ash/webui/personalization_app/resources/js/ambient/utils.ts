// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility functions to be used for ambient mode.
 */

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';

import {TopicSource} from '../../personalization_app.mojom-webui.js';

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
    default:
      console.warn('Invalid TopicSource value.');
      return '';
  }
}

/**
 * Returns a x-length dummy array of zeros (0s)
 */
export function getZerosArray(x: number): number[] {
  return new Array(x).fill(0);
}
