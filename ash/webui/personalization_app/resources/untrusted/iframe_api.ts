// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from '../common/assert.m.js';
import * as constants from '../common/constants.js';
import {isNonEmptyArray, isNullOrArray, isNullOrBigint} from '../common/utils.js';

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
    event: MessageEvent, expectedEventType: constants.EventType) {
  assert(
      event.origin === constants.trustedOrigin,
      'Message is not from the correct origin');
  assert(
      event.data.type === expectedEventType,
      `Expected event type: ${expectedEventType}`);

  const data: constants.Events = event.data;
  switch (data.type) {
    case constants.EventType.SEND_COLLECTIONS: {
      assert(isNonEmptyArray(data.collections), 'Expected collections array');
      return data.collections;
    }
    case constants.EventType.SEND_GOOGLE_PHOTOS_COUNT: {
      assert(isNullOrBigint(data.count), 'Expected photos count');
      return data.count;
    }
    case constants.EventType.SEND_GOOGLE_PHOTOS_PHOTOS: {
      assert(isNullOrArray(data.photos), 'Expected photos array');
      return data.photos;
    }
    case constants.EventType.SEND_LOCAL_IMAGE_DATA: {
      assert(typeof data.data === 'object', 'Expected data object');
      return data.data;
    }
    case constants.EventType.SEND_LOCAL_IMAGES:
      // Images array may be empty.
      assert(Array.isArray(data.images), 'Expected images array');
      return data.images;
    case constants.EventType.SEND_IMAGE_TILES: {
      // Images array may be empty.
      assert(Array.isArray(data.tiles), 'Expected images array');
      return data.tiles;
    }
    case constants.EventType.SEND_CURRENT_WALLPAPER_ASSET_ID:
    case constants.EventType.SEND_PENDING_WALLPAPER_ASSET_ID: {
      assert(data.assetId === null || typeof data.assetId === 'bigint');
      return data.assetId;
    }
    case constants.EventType.SEND_VISIBLE: {
      assert(typeof data.visible === 'boolean');
      return data.visible;
    }
    default:
      assertNotReached('Unknown event type');
  }
  return null;
}
