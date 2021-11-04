// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
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
 * @param {!ash.personalizationApp.mojom.WallpaperProviderInterface} provider
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
 * @param {!ash.personalizationApp.mojom.WallpaperProviderInterface} provider
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

  const allCollectionsImages =
      await Promise.all(collections.reduce((previousResult, collection) => {
        previousResult.push(provider.fetchImagesForCollection(collection.id));
        return previousResult;
      }, []));

  collections.forEach((collection, index) => {
    let {images} = allCollectionsImages[index];
    if (!isNonEmptyArray(images)) {
      console.warn('Failed to fetch images for collection id', collection.id);
      images = null;
    }
    store.dispatch(action.setImagesForCollectionAction(collection.id, images));
  });
}

/**
 * Gets the list of Google Photos albums and saves it to the store.
 * @param {!ash.personalizationApp.mojom.WallpaperProviderInterface} provider
 * @param {!PersonalizationStore} store
 */
async function getGooglePhotosAlbums(provider, store) {
  store.dispatch(action.beginLoadGooglePhotosAlbumsAction());

  // TODO(dmblack): Create and wire up mojo API. For now, simulate an async
  // request that returns an empty response list of Google Photos albums.
  return new Promise(resolve => setTimeout(() => {
                       store.dispatch(action.setGooglePhotosAlbumsAction([]));
                       resolve();
                     }, 1000));
}

/**
 * Gets the count of Google Photos photos and saves it to the store.
 * @param {!ash.personalizationApp.mojom.WallpaperProviderInterface} provider
 * @param {!PersonalizationStore} store
 */
async function getGooglePhotosCount(provider, store) {
  store.dispatch(action.beginLoadGooglePhotosCountAction());

  // TODO(dmblack): Create and wire up mojo API. For now, simulate an async
  // request that returns a count of 1,000 Google Photos photos.
  return new Promise(resolve => setTimeout(() => {
                       store.dispatch(action.setGooglePhotosCountAction(1000));
                       resolve();
                     }, 1000));
}

/**
 * Gets the list of Google Photos photos and saves it to the store.
 * @param {!ash.personalizationApp.mojom.WallpaperProviderInterface} provider
 * @param {!PersonalizationStore} store
 */
async function getGooglePhotosPhotos(provider, store) {
  store.dispatch(action.beginLoadGooglePhotosPhotosAction());

  // TODO(dmblack): Create and wire up mojo API. For now, simulate an async
  // request that returns a list of 1,000 Google Photos photos.
  return new Promise(resolve => setTimeout(() => {
                       store.dispatch(action.setGooglePhotosPhotosAction(
                           Array.from({length: 1000})));
                       resolve();
                     }, 1000));
}

/**
 * Get list of local images from disk and save it to the store.
 * @param {!ash.personalizationApp.mojom.WallpaperProviderInterface} provider
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
 * @param {!ash.personalizationApp.mojom.WallpaperProviderInterface} provider
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
 * @param {!ash.personalizationApp.mojom.WallpaperImage |
 *     !mojoBase.mojom.FilePath} image
 * @param {!ash.personalizationApp.mojom.WallpaperProviderInterface} provider
 * @param {!PersonalizationStore} store
 * @param {!ash.personalizationApp.mojom.WallpaperLayout=} layout
 */
export async function selectWallpaper(
    image, provider, store,
    layout = ash.personalizationApp.mojom.WallpaperLayout.kCenterCropped) {
  // Batch these changes together to reduce polymer churn as multiple state
  // fields change quickly.
  store.beginBatchUpdate();
  store.dispatch(action.beginSelectImageAction(image));
  store.dispatch(action.beginLoadSelectedImageAction());
  const {tabletMode} = await provider.isInTabletMode();
  const shouldPreview =
      tabletMode && loadTimeData.getBoolean('fullScreenPreviewEnabled');
  store.endBatchUpdate();
  const {success} = await (() => {
    if (image.hasOwnProperty('assetId')) {
      return provider.selectWallpaper(
          image.assetId, /*preview_mode=*/ shouldPreview);
    } else if (image.path) {
      return provider.selectLocalImage(
          /** @type {!mojoBase.mojom.FilePath} */ (image),
          /** @type {!ash.personalizationApp.mojom.WallpaperLayout} */ (layout),
          /*preview_mode=*/ shouldPreview);
    } else {
      console.warn('Image must be a local image or a WallpaperImage');
      return {success: false};
    }
  })();
  store.beginBatchUpdate();
  store.dispatch(action.endSelectImageAction(image, success));
  // Delay opening full screen preview until done loading. This looks better if
  // the image load takes a long time, otherwise the user will see the old
  // wallpaper image for a while.
  if (success && shouldPreview) {
    store.dispatch(action.setFullscreenEnabledAction(/*enabled=*/ true));
  }
  if (!success) {
    console.warn('Error setting wallpaper');
    store.dispatch(action.setSelectedImageAction(store.data.currentSelected));
  }
  store.endBatchUpdate();
}

/**
 * @param {!ash.personalizationApp.mojom.WallpaperLayout} layout
 * @param {!ash.personalizationApp.mojom.WallpaperProviderInterface} provider
 * @param {!PersonalizationStore} store
 */
export async function setCustomWallpaperLayout(layout, provider, store) {
  const image = store.data.currentSelected;
  assert(image.type === WallpaperType.kCustomized);
  assert(
      layout === WallpaperLayout.kCenter ||
      layout === WallpaperLayout.kCenterCropped);

  if (image.layout === layout) {
    return;
  }

  store.dispatch(action.beginLoadSelectedImageAction());
  await provider.setCustomWallpaperLayout(layout);
}

/**
 * @param {string} collectionId
 * @param {!ash.personalizationApp.mojom.WallpaperProviderInterface} provider
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
 * @param {!ash.personalizationApp.mojom.WallpaperProviderInterface} provider
 * @param {!PersonalizationStore} store
 */
export async function getDailyRefreshCollectionId(provider, store) {
  const {collectionId} = await provider.getDailyRefreshCollectionId();
  store.dispatch(action.setDailyRefreshCollectionIdAction(collectionId));
}

/**
 * Refresh the wallpaper. Noop if daily refresh is not enabled.
 * @param {!ash.personalizationApp.mojom.WallpaperProviderInterface} provider
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
 * @param {!ash.personalizationApp.mojom.WallpaperProviderInterface} provider
 */
export async function confirmPreviewWallpaper(provider) {
  await provider.confirmPreviewWallpaper();
}

/**
 * Cancel preview wallpaper and show the previous wallpaper.
 * @param {!ash.personalizationApp.mojom.WallpaperProviderInterface} provider
 */
export async function cancelPreviewWallpaper(provider) {
  await provider.cancelPreviewWallpaper();
}

/**
 * Fetches list of collections, then fetches list of images for each collection.
 * @param {!ash.personalizationApp.mojom.WallpaperProviderInterface} provider
 * @param {!PersonalizationStore} store
 */
export async function initializeBackdropData(provider, store) {
  await fetchCollections(provider, store);
  await fetchAllImagesForCollections(provider, store);
}

/**
 * Gets the initial Google Photos data state and saves it to the store.
 * @param {!ash.personalizationApp.mojom.WallpaperProviderInterface} provider
 * @param {!PersonalizationStore} store
 */
export async function initializeGooglePhotosData(provider, store) {
  await getGooglePhotosCount(provider, store);

  // If the count of Google Photos photos is zero or null, it's not necesssary
  // to query the server for the list of albums/photos.
  const count = store.data.googlePhotos.count;
  if (count === 0 || count === null) {
    const /** ?Array<undefined> */ result = count === 0 ? [] : null;
    store.beginBatchUpdate();
    store.dispatch(action.beginLoadGooglePhotosAlbumsAction());
    store.dispatch(action.setGooglePhotosAlbumsAction(result));
    store.dispatch(action.beginLoadGooglePhotosPhotosAction());
    store.dispatch(action.setGooglePhotosPhotosAction(result));
    store.endBatchUpdate();
    return;
  }

  await Promise.all([
    getGooglePhotosAlbums(provider, store),
    getGooglePhotosPhotos(provider, store),
  ]);
}

/**
 * Gets list of local images, then fetches image thumbnails for each local
 * image.
 * @param {!ash.personalizationApp.mojom.WallpaperProviderInterface} provider
 * @param {!PersonalizationStore} store
 */
export async function fetchLocalData(provider, store) {
  // Do not restart loading local image list if a load is already in progress.
  if (!store.data.loading.local.images) {
    await getLocalImages(provider, store);
  }
  await getMissingLocalImageThumbnails(provider, store);
}
