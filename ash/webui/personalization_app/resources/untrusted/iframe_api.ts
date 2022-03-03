// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as constants from '../common/constants.js';
import {isNullOrArray, isNullOrNumber} from '../common/utils.js';
import {onMessageReceived} from '../trusted/wallpaper/untrusted_message_handler.js';

/**
 * @fileoverview Helper functions for communicating between trusted and
 * untrusted. All untrusted -> trusted communication must happen through the
 * functions in this file.
 */

/**
 * Select a collection. Sent from untrusted to trusted.
 */
export function selectCollection(collectionId: string) {
  const event: constants.SelectCollectionEvent = {
    type: constants.EventType.SELECT_COLLECTION,
    collectionId
  };
  onMessageReceived(event);
}

/**
 * Select the Google Photos collection. Sent from untrusted to trusted.
 */
export function selectGooglePhotosCollection() {
  const event: constants.SelectGooglePhotosCollectionEvent = {
    type: constants.EventType.SELECT_GOOGLE_PHOTOS_COLLECTION
  };
  onMessageReceived(event);
}

/**
 * Select the local collection. Sent from untrusted to trusted.
 */
export function selectLocalCollection() {
  const event: constants.SelectLocalCollectionEvent = {
    type: constants.EventType.SELECT_LOCAL_COLLECTION
  };
  onMessageReceived(event);
}

/**
 * Select an image. Sent from untrusted to trusted.
 */
export function selectImage(assetId: bigint) {
  const event: constants.SelectImageEvent = {
    type: constants.EventType.SELECT_IMAGE,
    assetId
  };
  onMessageReceived(event);
}

/**
 * Called from untrusted code to validate that a received event is of an
 * expected type and contains the expected data.
 */
export function validateReceivedData(event: constants.Events): boolean {
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
