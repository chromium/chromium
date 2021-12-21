// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Helper functions for communicating between trusted and
 * untrusted. All trusted -> untrusted communication must happen through the
 * functions in this file.
 */

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import * as constants from '../common/constants.js';
import {isNonEmptyArray} from '../common/utils.js';

import {WallpaperCollection, WallpaperImage} from './personalization_app.mojom-webui.js';


/**
 * Send an array of wallpaper collections to untrusted.
 */
export function sendCollections(
    target: Window, collections: Array<WallpaperCollection>) {
  const event: constants.SendCollectionsEvent = {
    type: constants.EventType.SEND_COLLECTIONS,
    collections
  };
  target.postMessage(event, constants.untrustedOrigin);
}

/**
 * Sends the count of Google Photos photos to untrusted.
 */
export function sendGooglePhotosCount(target: Window, count: number|null) {
  const event: constants.SendGooglePhotosCountEvent = {
    type: constants.EventType.SEND_GOOGLE_PHOTOS_COUNT,
    count
  };
  target.postMessage(event, constants.untrustedOrigin);
}

/**
 * Sends the list of Google Photos photos to untrusted.
 */
export function sendGooglePhotosPhotos(
    target: Window, photos: Array<Url>|null) {
  const event: constants.SendGooglePhotosPhotosEvent = {
    type: constants.EventType.SEND_GOOGLE_PHOTOS_PHOTOS,
    photos
  };
  target.postMessage(event, constants.untrustedOrigin);
}

/**
 * Send a mapping of collectionId to the number of images in that collection.
 * A value of null for a given collection id represents that the collection
 * failed to load.
 */
export function sendImageCounts(
    target: Window, counts: {[key: string]: number|null}) {
  const event: constants.SendImageCountsEvent = {
    type: constants.EventType.SEND_IMAGE_COUNTS,
    counts
  };
  target.postMessage(event, constants.untrustedOrigin);
}

/**
 * Send visibility status to a target iframe. Currently used to trigger a
 * resize event on iron-list when an iframe becomes visible again so that
 * iron-list will run layout with the current size.
 */
export function sendVisible(target: Window, visible: boolean) {
  const event: constants.SendVisibleEvent = {
    type: constants.EventType.SEND_VISIBLE,
    visible
  };
  target.postMessage(event, constants.untrustedOrigin);
}

/**
 * Send an array of wallpaper images to chrome-untrusted://.
 * Will clear the page if images is empty array.
 */
export function sendImageTiles(target: Window, tiles: constants.ImageTile[]) {
  const event: constants.SendImageTilesEvent = {
    type: constants.EventType.SEND_IMAGE_TILES,
    tiles
  };
  target.postMessage(event, constants.untrustedOrigin);
}

/**
 * Send an array of local images to chrome-untrusted://.
 */
export function sendLocalImages(target: Window, images: FilePath[]) {
  const event: constants.SendLocalImagesEvent = {
    type: constants.EventType.SEND_LOCAL_IMAGES,
    images
  };
  target.postMessage(event, constants.untrustedOrigin);
}

/**
 * Sends image data keyed by stringified image id.
 */
export function sendLocalImageData(
    target: Window, data: {[key: string]: string}) {
  const event: constants.SendLocalImageDataEvent = {
    type: constants.EventType.SEND_LOCAL_IMAGE_DATA,
    data
  };
  target.postMessage(event, constants.untrustedOrigin);
}

/**
 * Send the |assetId| of the currently selected wallpaper to |target| iframe
 * window. Sending null indicates that no image is selected.
 */
export function sendCurrentWallpaperAssetId(
    target: Window, assetId: bigint|undefined) {
  const event: constants.SendCurrentWallpaperAssetIdEvent = {
    type: constants.EventType.SEND_CURRENT_WALLPAPER_ASSET_ID,
    assetId
  };
  target.postMessage(event, constants.untrustedOrigin);
}

/**
 * Send the |assetId| to the |target| iframe when the user clicks on online
 * wallpaper image.
 */
export function sendPendingWallpaperAssetId(
    target: Window, assetId: bigint|undefined) {
  const event: constants.SendPendingWallpaperAssetIdEvent = {
    type: constants.EventType.SEND_PENDING_WALLPAPER_ASSET_ID,
    assetId
  };
  target.postMessage(event, constants.untrustedOrigin);
}


/**
 * Called from trusted code to validate that a received postMessage event
 * contains valid data. Ignores messages that are not of the expected type.
 */
export function validateReceivedSelection(
    event: MessageEvent,
    choices: WallpaperCollection[]|null): WallpaperCollection;
export function validateReceivedSelection(
    event: MessageEvent, choices: WallpaperImage[]|null): WallpaperImage;
export function validateReceivedSelection(
    event: MessageEvent, choices: (WallpaperCollection|WallpaperImage)[]|null):
    WallpaperCollection|WallpaperImage {
  assert(isNonEmptyArray(choices), 'choices must be a non-empty array');

  const data: constants.Events = event.data;
  let selected: WallpaperCollection|WallpaperImage|undefined = undefined;
  switch (data.type) {
    case constants.EventType.SELECT_COLLECTION: {
      assert(!!data.collectionId, 'Expected a collection id parameter');
      selected = (choices as WallpaperCollection[])
                     .find(choice => choice.id === data.collectionId);
      break;
    }
    case constants.EventType.SELECT_IMAGE: {
      assert(
          data.hasOwnProperty('assetId'),
          'Expected an image assetId parameter');
      assert(
          typeof data.assetId === 'bigint', 'assetId parameter must be bigint');
      selected = (choices as WallpaperImage[])
                     .find(choice => choice.assetId === data.assetId);
      break;
    }
    default:
      assertNotReached('Unknown event type');
  }

  assert(!!selected, 'No valid selection found in choices');
  return selected!;
}
