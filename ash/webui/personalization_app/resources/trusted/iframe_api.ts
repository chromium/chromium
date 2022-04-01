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
import {CollectionsGrid} from '../untrusted/collections_grid.js';
import {ImagesGrid} from '../untrusted/images_grid.js';

import {GooglePhotosEnablementState, WallpaperCollection, WallpaperImage} from './personalization_app.mojom-webui.js';

/**
 * TODO(b:197023872) this class is deprecated and should be removed by more
 * post-iframe cleanup.
 */
export class IFrameApi {
  /**
   * Send an array of wallpaper collections to untrusted.
   */
  sendCollections(
      target: CollectionsGrid, collections: Array<WallpaperCollection>) {
    const event: constants.SendCollectionsEvent = {
      type: constants.EventType.SEND_COLLECTIONS,
      collections
    };
    target.onMessageReceived(event);
  }

  /**
   * Sends the count of Google Photos photos to untrusted.
   */
  sendGooglePhotosCount(target: CollectionsGrid, count: number|null) {
    const event: constants.SendGooglePhotosCountEvent = {
      type: constants.EventType.SEND_GOOGLE_PHOTOS_COUNT,
      count
    };
    target.onMessageReceived(event);
  }

  /**
   * Sends whether the user is allowed to access Google Photos to untrusted.
   */
  sendGooglePhotosEnabled(
      target: CollectionsGrid, enabled: GooglePhotosEnablementState) {
    const event: constants.SendGooglePhotosEnabledEvent = {
      type: constants.EventType.SEND_GOOGLE_PHOTOS_ENABLED,
      enabled
    };
    target.onMessageReceived(event);
  }

  /**
   * Sends the list of Google Photos photos to untrusted.
   */
  sendGooglePhotosPhotos(target: CollectionsGrid, photos: Array<Url>|null) {
    const event: constants.SendGooglePhotosPhotosEvent = {
      type: constants.EventType.SEND_GOOGLE_PHOTOS_PHOTOS,
      photos
    };
    target.onMessageReceived(event);
  }

  /**
   * Send a mapping of collectionId to the number of images in that collection.
   * A value of null for a given collection id represents that the collection
   * failed to load.
   */
  sendImageCounts(
      target: CollectionsGrid, counts: {[key: string]: number|null}) {
    const event: constants.SendImageCountsEvent = {
      type: constants.EventType.SEND_IMAGE_COUNTS,
      counts
    };
    target.onMessageReceived(event);
  }

  /**
   * Send visibility status to a target iframe. Currently used to trigger a
   * resize event on iron-list when an iframe becomes visible again so that
   * iron-list will run layout with the current size.
   */
  sendVisible(target: CollectionsGrid|ImagesGrid, visible: boolean) {
    const event: constants.SendVisibleEvent = {
      type: constants.EventType.SEND_VISIBLE,
      visible
    };
    target.onMessageReceived(event);
  }

  /**
   * Send an array of wallpaper images to chrome-untrusted://.
   * Will clear the page if images is empty array.
   */
  sendImageTiles(target: ImagesGrid, tiles: constants.ImageTile[]) {
    const event: constants.SendImageTilesEvent = {
      type: constants.EventType.SEND_IMAGE_TILES,
      tiles
    };
    target.onMessageReceived(event);
  }

  /**
   * Send an array of local images to chrome-untrusted://.
   */
  sendLocalImages(target: CollectionsGrid, images: FilePath[]) {
    const event: constants.SendLocalImagesEvent = {
      type: constants.EventType.SEND_LOCAL_IMAGES,
      images
    };
    target.onMessageReceived(event);
  }

  /**
   * Sends image data keyed by stringified image id.
   */
  sendLocalImageData(target: CollectionsGrid, data: Record<string, string>) {
    const event: constants.SendLocalImageDataEvent = {
      type: constants.EventType.SEND_LOCAL_IMAGE_DATA,
      data
    };
    target.onMessageReceived(event);
  }

  /**
   * Send the |assetId| of the currently selected wallpaper to |target|.
   * Sending null indicates that no image is selected.
   */
  sendCurrentWallpaperAssetId(target: ImagesGrid, assetId: bigint|undefined) {
    const event: constants.SendCurrentWallpaperAssetIdEvent = {
      type: constants.EventType.SEND_CURRENT_WALLPAPER_ASSET_ID,
      assetId
    };
    target.onMessageReceived(event);
  }

  /**
   * Send the |assetId| to the |target| when the user clicks on online wallpaper
   * image.
   */
  sendPendingWallpaperAssetId(target: ImagesGrid, assetId: bigint|undefined) {
    const event: constants.SendPendingWallpaperAssetIdEvent = {
      type: constants.EventType.SEND_PENDING_WALLPAPER_ASSET_ID,
      assetId
    };
    target.onMessageReceived(event);
  }


  /**
   * Called from trusted code to validate that a received event contains valid
   * data. Ignores messages that are not of the expected type.
   */
  validateReceivedSelection(
      data: constants.Events,
      choices: WallpaperCollection[]|null): WallpaperCollection;
  validateReceivedSelection(
      data: constants.Events, choices: WallpaperImage[]|null): WallpaperImage;
  validateReceivedSelection(
      data: constants.Events,
      choices: (WallpaperCollection|WallpaperImage)[]|null): WallpaperCollection
      |WallpaperImage {
    assert(isNonEmptyArray(choices), 'choices must be a non-empty array');

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
            typeof data.assetId === 'bigint',
            'assetId parameter must be bigint');
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

  static getInstance(): IFrameApi {
    return instance || (instance = new IFrameApi());
  }

  static setInstance(obj: IFrameApi) {
    instance = obj;
  }
}

let instance: IFrameApi|null = null;
