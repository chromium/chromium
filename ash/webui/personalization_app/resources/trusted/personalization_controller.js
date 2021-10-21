// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js'
import {isNonEmptyArray} from '../common/utils.js';
import * as action from './personalization_actions.js';
import {WallpaperLayout, WallpaperType} from './personalization_reducers.js';
import {PersonalizationStore} from './personalization_store.js';

/**
 * @fileoverview contains all of the functions to interact with C++ side through
 * mojom calls. Handles setting |PersonalizationStore| state in response to
 * mojom data.
 * TODO(b/181697575) handle errors and allow user to retry these functions.
 */

/**
 * Fetch wallpaper collections and save them to the store.
 * @param {!chromeos.personalizationApp.mojom.WallpaperProviderInterface}
 *     provider
 * @param {!PersonalizationStore} store
 */
async function fetchCollections(provider, store) {
  let {collections} = await provider.fetchCollections();
  if (!isNonEmptyArray(collections)) {
    console.warn('Failed to fetch wallpaper collections');
    collections = null;
  }
  store.dispatch(action.setCollectionsAction(collections));
}

/**
 * Fetch all of the wallpaper collections one at a time.
 * @param {!chromeos.personalizationApp.mojom.WallpaperProviderInterface}
 *     provider
 * @param {!PersonalizationStore} store
 */
async function fetchAllImagesForCollections(provider, store) {
  const collections = store.data.backdrop.collections;
  if (!Array.isArray(collections)) {
    console.warn(
        'Cannot fetch data for collections when it is not initialized');
    return;
  }
  store.dispatch(action.beginLoadImagesForCollectionsAction(collections));
  for (const {id} of /** @type {!Array<{id: string}>} */ (collections)) {
    let {images} = await provider.fetchImagesForCollection(id);
    if (!isNonEmptyArray(images)) {
      console.warn('Failed to fetch images for collection id', id);
      images = null;
    }
    store.dispatch(action.setImagesForCollectionAction(id, images));
  }
}

/**
 * Get list of local images from disk and save it to the store.
 * @param {!chromeos.personalizationApp.mojom.WallpaperProviderInterface}
 *     provider
 * @param {!PersonalizationStore} store
 */
async function getLocalImages(provider, store) {
  store.dispatch(action.beginLoadLocalImagesAction());
  const {images} = await provider.getLocalImages();
  if (images == null) {
    console.warn('Failed to fetch local images');
  }
  store.dispatch(action.setLocalImagesAction(images));
}

/**
 * Because thumbnail loading can happen asynchronously and is triggered
 * on page load and on window focus, multiple "threads" can be fetching
 * thumbnails simultaneously. Synchronize them with a task queue.
 * @type {Set<string>}
 */
const imageThumbnailsToFetch = new Set();

/**
 * Get an image thumbnail one at a time for every local image that does not have
 * a thumbnail yet.
 * @param {!chromeos.personalizationApp.mojom.WallpaperProviderInterface}
 *     provider
 * @param {!PersonalizationStore} store
 */
async function getMissingLocalImageThumbnails(provider, store) {
  if (!Array.isArray(store.data.local.images)) {
    console.warn('Cannot fetch thumbnails with invalid image list');
    return;
  }

  // Set correct loading state for each image thumbnail. Do in a batch update to
  // reduce number of times that polymer must re-render.
  store.beginBatchUpdate();
  for (const filePath of store.data.local.images) {
    if (store.data.local.data[filePath.path] ||
        store.data.loading.local.data[filePath.path] ||
        imageThumbnailsToFetch.has(filePath.path)) {
      // Do not re-load thumbnail if already present, or already loading.
      continue;
    }
    imageThumbnailsToFetch.add(filePath.path);
    store.dispatch(action.beginLoadLocalImageDataAction(filePath));
  }
  store.endBatchUpdate();

  // There may be multiple async tasks triggered that pull off this queue.
  while (imageThumbnailsToFetch.size) {
    const path = imageThumbnailsToFetch.values().next().value;
    imageThumbnailsToFetch.delete(path);
    const {data} = await provider.getLocalImageThumbnail({path});
    if (!data) {
      console.warn('Failed to fetch local image data', path);
    }
    store.dispatch(action.setLocalImageDataAction({path}, data));
  }
}

/**
 * @param {!chromeos.personalizationApp.mojom.WallpaperImage |
 *     !mojoBase.mojom.FilePath} image
 * @param {!chromeos.personalizationApp.mojom.WallpaperProviderInterface}
 *     provider
 * @param {!PersonalizationStore} store
 */
export async function selectWallpaper(image, provider, store) {
  // Batch these changes together to reduce polymer churn as multiple state
  // fields change quickly.
  store.beginBatchUpdate();
  store.dispatch(action.beginSelectImageAction(image));
  store.dispatch(action.beginLoadSelectedImageAction());
  const {tabletMode} = await provider.isInTabletMode();
  const shouldPreview =
      tabletMode && loadTimeData.getBoolean('fullScreenPreviewEnabled');
  if (shouldPreview) {
    store.dispatch(action.setFullscreenEnabledAction(/*enabled=*/ true))
  }
  store.endBatchUpdate();
  const {success} = await (() => {
    if (image.hasOwnProperty('assetId')) {
      return provider.selectWallpaper(
          image.assetId, /*preview_mode=*/ shouldPreview);
    } else if (image.path) {
      return provider.selectLocalImage(
          /** @type {!mojoBase.mojom.FilePath} */ (image),
          /*preview_mode=*/ shouldPreview);
    } else {
      console.warn('Image must be a local image or a WallpaperImage');
      return {success: false};
    }
  })();
  store.beginBatchUpdate();
  store.dispatch(action.endSelectImageAction(image, success));
  if (!success) {
    console.warn('Error setting wallpaper');
    store.dispatch(action.setSelectedImageAction(store.data.currentSelected));
  }
  store.endBatchUpdate();
}

/**
 * @param {!chromeos.personalizationApp.mojom.WallpaperLayout} layout
 * @param {!chromeos.personalizationApp.mojom.WallpaperProviderInterface}
 *     provider
 * @param {!PersonalizationStore} store
 */
export async function setCustomWallpaperLayout(layout, provider, store) {
  const image = store.data.currentSelected;
  assert(image.type === WallpaperType.kCustomized);
  assert(
      layout === WallpaperLayout.kCenter ||
      layout === WallpaperLayout.kCenterCropped);

  if (image.layout === layout)
    return;

  store.dispatch(action.beginLoadSelectedImageAction());
  await provider.setCustomWallpaperLayout(layout);
}

/**
 * @param {string} collectionId
 * @param {!chromeos.personalizationApp.mojom.WallpaperProviderInterface}
 *     provider
 * @param {!PersonalizationStore} store
 */
export async function setDailyRefreshCollectionId(
    collectionId, provider, store) {
  await provider.setDailyRefreshCollectionId(collectionId);
  // Dispatch action to highlight enabled daily refresh.
  getDailyRefreshCollectionId(provider, store);
}

/**
 * Get the daily refresh collection id. It can be empty if daily refresh is not
 * enabled.
 * @param {!chromeos.personalizationApp.mojom.WallpaperProviderInterface}
 *     provider
 * @param {!PersonalizationStore} store
 */
export async function getDailyRefreshCollectionId(provider, store) {
  const {collectionId} = await provider.getDailyRefreshCollectionId();
  store.dispatch(action.setDailyRefreshCollectionIdAction(collectionId));
}

/**
 * Refresh the wallpaper. Noop if daily refresh is not enabled.
 * @param {!chromeos.personalizationApp.mojom.WallpaperProviderInterface}
 *     provider
 * @param {!PersonalizationStore} store
 */
export async function updateDailyRefreshWallpaper(provider, store) {
  store.dispatch(action.beginUpdateDailyRefreshImageAction());
  store.dispatch(action.beginLoadSelectedImageAction());
  const {success} = await provider.updateDailyRefreshWallpaper();
  if (success) {
    store.dispatch(action.setUpdatedDailyRefreshImageAction());
  }
}

/**
 * Confirm and set preview wallpaper as actual wallpaper.
 * @param {!chromeos.personalizationApp.mojom.WallpaperProviderInterface}
 *     provider
 */
export async function confirmPreviewWallpaper(provider) {
  await provider.confirmPreviewWallpaper();
}

/**
 * Cancel preview wallpaper and show the previous wallpaper.
 * @param {!chromeos.personalizationApp.mojom.WallpaperProviderInterface}
 *     provider
 */
export async function cancelPreviewWallpaper(provider) {
  await provider.cancelPreviewWallpaper();
}

/**
 * Fetches list of collections, then fetches list of images for each collection.
 * @param {!chromeos.personalizationApp.mojom.WallpaperProviderInterface}
 *     provider
 * @param {!PersonalizationStore} store
 */
export async function initializeBackdropData(provider, store) {
  await fetchCollections(provider, store);
  await fetchAllImagesForCollections(provider, store);
}

/**
 * Gets list of local images, then fetches image thumbnails for each local
 * image.
 * @param {!chromeos.personalizationApp.mojom.WallpaperProviderInterface}
 *     provider
 * @param {!PersonalizationStore} store
 */
export async function fetchLocalData(provider, store) {
  // Do not restart loading local image list if a load is already in progress.
  if (!store.data.loading.local.images) {
    await getLocalImages(provider, store);
  }
  await getMissingLocalImageThumbnails(provider, store);
}
