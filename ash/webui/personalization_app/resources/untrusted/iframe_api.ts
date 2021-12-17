// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '../common/assert.m.js';
import * as constants from '../common/constants.js';
import {isNonEmptyArray, isNullOrArray, isNullOrNumber} from '../common/utils.js';

/**
 * @fileoverview Helper functions for communicating between trusted and
 * untrusted. All untrusted -> trusted communication must happen through the
 * functions in this file.
 */

/**
 * Select a collection. Sent from untrusted to trusted.
 */
export function selectCollection(target: Window, collectionId: string) {
  const event: constants.SelectCollectionEvent = {
    type: constants.EventType.SELECT_COLLECTION,
    collectionId
  };
  target.postMessage(event, constants.trustedOrigin);
}

/**
 * Select the Google Photos collection. Sent from untrusted to trusted.
 */
export function selectGooglePhotosCollection(target: Window) {
  const event: constants.SelectGooglePhotosCollectionEvent = {
    type: constants.EventType.SELECT_GOOGLE_PHOTOS_COLLECTION
  };
  target.postMessage(event, constants.trustedOrigin);
}

/**
 * Select the local collection. Sent from untrusted to trusted.
 */
export function selectLocalCollection(target: Window) {
  const event: constants.SelectLocalCollectionEvent = {
    type: constants.EventType.SELECT_LOCAL_COLLECTION
  };
  target.postMessage(event, constants.trustedOrigin);
}

/**
 * Select an image. Sent from untrusted to trusted.
 */
export function selectImage(target: Window, assetId: bigint) {
  const event: constants.SelectImageEvent = {
    type: constants.EventType.SELECT_IMAGE,
    assetId
  };
  target.postMessage(event, constants.trustedOrigin);
}

/**
 * Called from untrusted code to validate that a received event is of an
 * expected type and contains the expected data.
 */
export function validateReceivedData(
    event: constants.Events, origin: string): boolean {
  assert(
      origin === constants.trustedOrigin,
      'Message is not from the correct origin');

  switch (event.type) {
    case constants.EventType.SEND_COLLECTIONS: {
      return isNullOrArray(event.collections);
    }
    case constants.EventType.SEND_GOOGLE_PHOTOS_COUNT: {
      return isNullOrNumber(event.count);
    }
    case constants.EventType.SEND_GOOGLE_PHOTOS_PHOTOS: {
      return isNullOrArray(event.photos);
    }
    case constants.EventType.SEND_IMAGE_COUNTS:
      return typeof event.counts === 'object';
    case constants.EventType.SEND_LOCAL_IMAGE_DATA: {
      return typeof event.data === 'object';
    }
    case constants.EventType.SEND_LOCAL_IMAGES:
      // Images array may be empty.
      return Array.isArray(event.images);
    case constants.EventType.SEND_IMAGE_TILES: {
      // Images array may be empty.
      return Array.isArray(event.tiles);
    }
    case constants.EventType.SEND_CURRENT_WALLPAPER_ASSET_ID:
    case constants.EventType.SEND_PENDING_WALLPAPER_ASSET_ID: {
      return event.assetId === null || typeof event.assetId === 'bigint';
    }
    case constants.EventType.SEND_VISIBLE: {
      return typeof event.visible === 'boolean';
    }
    default:
      return false;
  }
}
