// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {Action} from 'chrome://resources/js/cr/ui/store.js';
import {DisplayableImage} from './personalization_reducers.js';

/**
 * @fileoverview Defines the actions to change state.
 */

/** @enum {string} */
export const ActionName = {
  BEGIN_LOAD_GOOGLE_PHOTOS_ALBUMS: 'begin_load_google_photos_albums',
  BEGIN_LOAD_GOOGLE_PHOTOS_COUNT: 'begin_load_google_photos_count',
  BEGIN_LOAD_GOOGLE_PHOTOS_PHOTOS: 'begin_load_google_photos_photos',
  BEGIN_LOAD_IMAGES_FOR_COLLECTIONS: 'begin_load_images_for_collections',
  BEGIN_LOAD_LOCAL_IMAGES: 'begin_load_local_images',
  BEGIN_LOAD_LOCAL_IMAGE_DATA: 'begin_load_local_image_data',
  BEGIN_LOAD_SELECTED_IMAGE: 'begin_load_selected_image',
  BEGIN_SELECT_IMAGE: 'begin_select_image',
  BEGIN_UPDATE_DAILY_REFRESH_IMAGE: 'begin_update_daily_refresh_image',
  END_SELECT_IMAGE: 'end_select_image',
  SET_COLLECTIONS: 'set_collections',
  SET_DAILY_REFRESH_COLLECTION_ID: 'set_daily_refresh_collection_id',
  SET_GOOGLE_PHOTOS_ALBUMS: 'set_google_photos_albums',
  SET_GOOGLE_PHOTOS_COUNT: 'set_google_photos_count',
  SET_GOOGLE_PHOTOS_PHOTOS: 'set_google_photos_photos',
  SET_IMAGES_FOR_COLLECTION: 'set_images_for_collection',
  SET_LOCAL_IMAGES: 'set_local_images',
  SET_LOCAL_IMAGE_DATA: 'set_local_image_data',
  SET_SELECTED_IMAGE: 'set_selected_image',
  SET_UPDATED_DAILY_REFRESH_IMAGE: 'set_updated_daily_refreshed_image',
  DISMISS_ERROR: 'dismiss_error',
  SET_FULLSCREEN_ENABLED: 'set_fullscreen_enabled',
};

/**
 * Notify that the app is loading the list of Google Photos albums.
 * @return {!Action}
 */
export function beginLoadGooglePhotosAlbumsAction() {
  return {name: ActionName.BEGIN_LOAD_GOOGLE_PHOTOS_ALBUMS};
}

/**
 * Notify that the app is loading the count of Google Photos photos.
 * @return {!Action}
 */
export function beginLoadGooglePhotosCountAction() {
  return {name: ActionName.BEGIN_LOAD_GOOGLE_PHOTOS_COUNT};
}

/**
 * Notify that the app is loading the list of Google Photos photos.
 * @return {!Action}
 */
export function beginLoadGooglePhotosPhotosAction() {
  return {name: ActionName.BEGIN_LOAD_GOOGLE_PHOTOS_PHOTOS};
}

/**
 * Notify that app is loading image list for the given collection.
 * @param {?Array<!ash.personalizationApp.mojom.WallpaperCollection>}
 *     collections
 * @return {!Action}
 */
export function beginLoadImagesForCollectionsAction(collections) {
  return {
    collections,
    name: ActionName.BEGIN_LOAD_IMAGES_FOR_COLLECTIONS,
  };
}

/**
 * Notify that app is loading local image list.
 * @return {!Action}
 */
export function beginLoadLocalImagesAction() {
  return {name: ActionName.BEGIN_LOAD_LOCAL_IMAGES};
}

/**
 * Notify that app is loading thumbnail for the given local image.
 * @param {!mojoBase.mojom.FilePath} image
 * @return {!Action}
 */
export function beginLoadLocalImageDataAction(image) {
  return {
    id: image.path,
    name: ActionName.BEGIN_LOAD_LOCAL_IMAGE_DATA,
  };
}

/**
 * Notify that a user has clicked on the refresh button.
 * @return {!Action}
 */
export function beginUpdateDailyRefreshImageAction() {
  return {
    name: ActionName.BEGIN_UPDATE_DAILY_REFRESH_IMAGE,
  };
}

/**
 * Notify that app is loading currently selected image information.
 * @return {!Action}
 */
export function beginLoadSelectedImageAction() {
  return {name: ActionName.BEGIN_LOAD_SELECTED_IMAGE};
}

/**
 * Notify that a user has clicked on an image to set as wallpaper.
 * @param {!DisplayableImage} image
 * @return {!Action}
 */
export function beginSelectImageAction(image) {
  return {name: ActionName.BEGIN_SELECT_IMAGE, image};
}

/**
 * Notify that the user-initiated action to set image has finished.
 * @param {!DisplayableImage} image
 * @param {boolean} success
 * @return {!Action}
 */
export function endSelectImageAction(image, success) {
  return {name: ActionName.END_SELECT_IMAGE, image, success};
}

/**
 * Set the collections. May be called with null if an error occurred.
 * @param {?Array<!ash.personalizationApp.mojom.WallpaperCollection>}
 *     collections
 * @return {!Action}
 */
export function setCollectionsAction(collections) {
  return {
    collections,
    name: ActionName.SET_COLLECTIONS,
  };
}

/**
 * Set and enable daily refresh for given collectionId.
 * @param {?string} collectionId
 * @return {!Action}
 */
export function setDailyRefreshCollectionIdAction(collectionId) {
  return {
    collectionId,
    name: ActionName.SET_DAILY_REFRESH_COLLECTION_ID,
  };
}

/**
 * Sets the list of Google Photos albums. May be called with null on error.
 * @param {?Array<undefined>} albums
 * @return {!Action}
 */
export function setGooglePhotosAlbumsAction(albums) {
  return {albums, name: ActionName.SET_GOOGLE_PHOTOS_ALBUMS};
}

/**
 * Sets the count of Google Photos photos. May be called with null on error.
 * @param {?number} count
 * @return {!Action}
 */
export function setGooglePhotosCountAction(count) {
  return {count, name: ActionName.SET_GOOGLE_PHOTOS_COUNT};
}

/**
 * Sets the list of Google Photos photos. May be called with null on error.
 * @param {?Array<undefined>} photos
 * @return {!Action}
 */
export function setGooglePhotosPhotosAction(photos) {
  return {photos, name: ActionName.SET_GOOGLE_PHOTOS_PHOTOS};
}

/**
 * Set the images for a given collection. May be called with null if an error
 * occurred.
 * @param {string} collectionId
 * @param {?Array<!ash.personalizationApp.mojom.WallpaperImage>} images
 * @returns
 */
export function setImagesForCollectionAction(collectionId, images) {
  return {
    collectionId,
    images,
    name: ActionName.SET_IMAGES_FOR_COLLECTION,
  };
}

/**
 * Set the thumbnail data for a local image.
 * @param {!mojoBase.mojom.FilePath} filePath
 * @param {string} data
 * @return {!Action}
 */
export function setLocalImageDataAction(filePath, data) {
  return {
    id: filePath.path,
    data,
    name: ActionName.SET_LOCAL_IMAGE_DATA,
  };
}

/**
 * Set the list of local images.
 * @param {?Array<!mojoBase.mojom.FilePath>} images
 * @return {!Action}
 */
export function setLocalImagesAction(images) {
  return {
    images,
    name: ActionName.SET_LOCAL_IMAGES,
  };
}

/**
 * Notify that a image has been refreshed.
 * @return {!Action}
 */
export function setUpdatedDailyRefreshImageAction() {
  return {
    name: ActionName.SET_UPDATED_DAILY_REFRESH_IMAGE,
  };
}

/**
 * Returns an action to set the current image as currently selected across the
 * app. Can be called with null to represent no image currently selected or that
 * an error occurred.
 * @param {?ash.personalizationApp.mojom.CurrentWallpaper} image
 * @return {!Action}
 */
export function setSelectedImageAction(image) {
  return {
    image,
    name: ActionName.SET_SELECTED_IMAGE,
  };
}

/**
 * @return {!Action}
 */
export function dismissErrorAction() {
  return {name: ActionName.DISMISS_ERROR};
}

/**
 * @param {boolean} enabled
 * @return {!Action}
 */
export function setFullscreenEnabledAction(enabled) {
  assert(typeof enabled === 'boolean');
  return {name: ActionName.SET_FULLSCREEN_ENABLED, enabled};
}
