// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';

import {isNonEmptyArray} from '../../common/utils.js';
import {GooglePhotosAlbum, GooglePhotosEnablementState, GooglePhotosPhoto, WallpaperCollection, WallpaperImage, WallpaperLayout, WallpaperProviderInterface, WallpaperType} from '../personalization_app.mojom-webui.js';
import {PersonalizationStore} from '../personalization_store.js';
import {appendMaxResolutionSuffix, isFilePath, isGooglePhotosPhoto, isWallpaperImage} from '../utils.js';

import * as action from './wallpaper_actions.js';

/**
 * @fileoverview contains all of the functions to interact with C++ side through
 * mojom calls. Handles setting |PersonalizationStore| state in response to
 * mojom data.
 */

/** Fetch wallpaper collections and save them to the store. */
export async function fetchCollections(
    provider: WallpaperProviderInterface,
    store: PersonalizationStore): Promise<void> {
  let {collections} = await provider.fetchCollections();
  if (!isNonEmptyArray(collections)) {
    console.warn('Failed to fetch wallpaper collections');
    collections = null;
  }
  store.dispatch(action.setCollectionsAction(collections));
}

/** Helper function to fetch and dispatch images for a single collection. */
async function fetchAndDispatchCollectionImages(
    provider: WallpaperProviderInterface, store: PersonalizationStore,
    collection: WallpaperCollection): Promise<void> {
  let {images} = await provider.fetchImagesForCollection(collection.id);
  if (!isNonEmptyArray(images)) {
    console.warn('Failed to fetch images for collection id', collection.id);
    images = null;
  }
  store.dispatch(action.setImagesForCollectionAction(collection.id, images));
}

/** Fetch all of the wallpaper collection images in parallel. */
async function fetchAllImagesForCollections(
    provider: WallpaperProviderInterface,
    store: PersonalizationStore): Promise<void> {
  const collections = store.data.wallpaper.backdrop.collections;
  if (!Array.isArray(collections)) {
    console.warn(
        'Cannot fetch data for collections when it is not initialized');
    return;
  }

  store.dispatch(action.beginLoadImagesForCollectionsAction(collections));

  await Promise.all(collections.map(
      collection =>
          fetchAndDispatchCollectionImages(provider, store, collection)));
}

/**
 * Fetches the list of Google Photos photos for the album associated with the
 * specified id and saves it to the store.
 */
export async function fetchGooglePhotosAlbum(
    provider: WallpaperProviderInterface, store: PersonalizationStore,
    albumId: string): Promise<void> {
  // Photos should only be fetched after confirming access is allowed.
  const enabled = store.data.wallpaper.googlePhotos.enabled;
  assert(enabled === GooglePhotosEnablementState.kEnabled);

  store.dispatch(action.beginLoadGooglePhotosAlbumAction(albumId));

  let photos: Array<GooglePhotosPhoto>|null = [];
  let resumeToken =
      store.data.wallpaper.googlePhotos.resumeTokens.photosByAlbumId[albumId] ||
      null;

  const {response} = await provider.fetchGooglePhotosPhotos(
      /*itemId=*/ null, albumId, resumeToken);
  if (Array.isArray(response.photos)) {
    photos.push(...response.photos);
    resumeToken = response.resumeToken || null;
  } else {
    console.warn('Failed to fetch Google Photos album');
    photos = null;
    // NOTE: `resumeToken` is intentionally *not* modified so that the request
    // which failed can be reattempted.
  }

  // Impose max resolution.
  if (photos !== null) {
    photos = photos.map(
        photo => ({...photo, url: appendMaxResolutionSuffix(photo.url)}));
  }

  store.dispatch(
      action.appendGooglePhotosAlbumAction(albumId, photos, resumeToken));
}

/** Fetches the list of Google Photos albums and saves it to the store. */
export async function fetchGooglePhotosAlbums(
    provider: WallpaperProviderInterface,
    store: PersonalizationStore): Promise<void> {
  // Albums should only be fetched after confirming access is allowed.
  const enabled = store.data.wallpaper.googlePhotos.enabled;
  assert(enabled === GooglePhotosEnablementState.kEnabled);

  store.dispatch(action.beginLoadGooglePhotosAlbumsAction());

  let albums: Array<GooglePhotosAlbum>|null = [];
  let resumeToken = store.data.wallpaper.googlePhotos.resumeTokens.albums;

  const {response} = await provider.fetchGooglePhotosAlbums(resumeToken);
  if (Array.isArray(response.albums)) {
    albums.push(...response.albums);
    resumeToken = response.resumeToken || null;
  } else {
    console.warn('Failed to fetch Google Photos albums');
    albums = null;
    // NOTE: `resumeToken` is intentionally *not* modified so that the request
    // which failed can be reattempted.
  }

  // Impose max resolution.
  if (albums !== null) {
    albums = albums.map(
        album =>
            ({...album, preview: appendMaxResolutionSuffix(album.preview)}));
  }

  store.dispatch(action.appendGooglePhotosAlbumsAction(albums, resumeToken));
}

/** Fetches whether the user is allowed to access Google Photos. */
async function fetchGooglePhotosEnabled(
    provider: WallpaperProviderInterface,
    store: PersonalizationStore): Promise<void> {
  // Whether access is allowed should only be fetched once.
  assert(store.data.wallpaper.googlePhotos.enabled === undefined);

  store.dispatch(action.beginLoadGooglePhotosEnabledAction());
  const {state} = await provider.fetchGooglePhotosEnabled();
  if (state === GooglePhotosEnablementState.kError) {
    console.warn('Failed to fetch Google Photos enabled');
  }
  store.dispatch(action.setGooglePhotosEnabledAction(state));
}

/** Fetches the count of Google Photos photos and saves it to the store. */
async function fetchGooglePhotosCount(
    provider: WallpaperProviderInterface,
    store: PersonalizationStore): Promise<void> {
  // Count should only be fetched after confirming access is allowed.
  const enabled = store.data.wallpaper.googlePhotos.enabled;
  assert(enabled === GooglePhotosEnablementState.kEnabled);

  store.dispatch(action.beginLoadGooglePhotosCountAction());
  const {count} = await provider.fetchGooglePhotosCount();
  store.dispatch(action.setGooglePhotosCountAction(count >= 0 ? count : null));
}

/** Fetches the list of Google Photos photos and saves it to the store. */
export async function fetchGooglePhotosPhotos(
    provider: WallpaperProviderInterface,
    store: PersonalizationStore): Promise<void> {
  // Photos should only be fetched after confirmed access is allowed.
  const enabled = store.data.wallpaper.googlePhotos.enabled;
  assert(enabled === GooglePhotosEnablementState.kEnabled);

  store.dispatch(action.beginLoadGooglePhotosPhotosAction());

  let photos: Array<GooglePhotosPhoto>|null = [];
  let resumeToken = store.data.wallpaper.googlePhotos.resumeTokens.photos;

  const {response} = await provider.fetchGooglePhotosPhotos(
      /*itemId=*/ null, /*albumId=*/ null, resumeToken);
  if (Array.isArray(response.photos)) {
    photos.push(...response.photos);
    resumeToken = response.resumeToken || null;
  } else {
    console.warn('Failed to fetch Google Photos photos');
    photos = null;
    // NOTE: `resumeToken` is intentionally *not* modified so that the request
    // which failed can be reattempted.
  }

  // Impose max resolution.
  if (photos !== null) {
    photos = photos.map(
        photo => ({...photo, url: appendMaxResolutionSuffix(photo.url)}));
  }

  store.dispatch(action.appendGooglePhotosPhotosAction(photos, resumeToken));
}

/** Get list of local images from disk and save it to the store. */
export async function getLocalImages(
    provider: WallpaperProviderInterface,
    store: PersonalizationStore): Promise<void> {
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
 */
const imageThumbnailsToFetch = new Set<FilePath['path']>();

/**
 * Get an image thumbnail one at a time for every local image that does not have
 * a thumbnail yet.
 */
async function getMissingLocalImageThumbnails(
    provider: WallpaperProviderInterface,
    store: PersonalizationStore): Promise<void> {
  if (!Array.isArray(store.data.wallpaper.local.images)) {
    console.warn('Cannot fetch thumbnails with invalid image list');
    return;
  }

  // Set correct loading state for each image thumbnail. Do in a batch update to
  // reduce number of times that polymer must re-render.
  store.beginBatchUpdate();
  for (const filePath of store.data.wallpaper.local.images) {
    if (store.data.wallpaper.local.data[filePath.path] ||
        store.data.wallpaper.loading.local.data[filePath.path] ||
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

export async function selectWallpaper(
    image: WallpaperImage|FilePath|GooglePhotosPhoto,
    provider: WallpaperProviderInterface, store: PersonalizationStore,
    layout: WallpaperLayout = WallpaperLayout.kCenterCropped): Promise<void> {
  // Batch these changes together to reduce polymer churn as multiple state
  // fields change quickly.
  store.beginBatchUpdate();
  store.dispatch(action.beginSelectImageAction(image));
  store.dispatch(action.beginLoadSelectedImageAction());
  const {tabletMode} = await provider.isInTabletMode();
  const shouldPreview =
      tabletMode && loadTimeData.getBoolean('fullScreenPreviewEnabled');
  if (shouldPreview) {
    provider.makeTransparent();
  }
  store.endBatchUpdate();
  const {success} = await (() => {
    if (isWallpaperImage(image)) {
      return provider.selectWallpaper(
          image.assetId, /*preview_mode=*/ shouldPreview);
    } else if (isFilePath(image)) {
      return provider.selectLocalImage(
          image, layout, /*preview_mode=*/ shouldPreview);
    } else if (isGooglePhotosPhoto(image)) {
      return provider.selectGooglePhotosPhoto(
          image.id, layout, /*preview_mode=*/ shouldPreview);
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
    store.dispatch(
        action.setSelectedImageAction(store.data.wallpaper.currentSelected));
  }
  store.endBatchUpdate();
}

export async function setCurrentWallpaperLayout(
    layout: WallpaperLayout, provider: WallpaperProviderInterface,
    store: PersonalizationStore): Promise<void> {
  const image = store.data.wallpaper.currentSelected;
  assert(image);
  assert(
      image.type === WallpaperType.kCustomized ||
      image.type === WallpaperType.kGooglePhotos);
  assert(
      layout === WallpaperLayout.kCenter ||
      layout === WallpaperLayout.kCenterCropped);

  if (image.layout === layout) {
    return;
  }

  store.dispatch(action.beginLoadSelectedImageAction());
  await provider.setCurrentWallpaperLayout(layout);
}

export async function setDailyRefreshCollectionId(
    collectionId: WallpaperCollection['id'],
    provider: WallpaperProviderInterface,
    store: PersonalizationStore): Promise<void> {
  await provider.setDailyRefreshCollectionId(collectionId);
  // Dispatch action to highlight enabled daily refresh.
  getDailyRefreshCollectionId(provider, store);
}

/**
 * Get the daily refresh collection id. It can be empty if daily refresh is not
 * enabled.
 */
export async function getDailyRefreshCollectionId(
    provider: WallpaperProviderInterface,
    store: PersonalizationStore): Promise<void> {
  const {collectionId} = await provider.getDailyRefreshCollectionId();
  store.dispatch(action.setDailyRefreshCollectionIdAction(collectionId));
}

/** Refresh the wallpaper. Noop if daily refresh is not enabled. */
export async function updateDailyRefreshWallpaper(
    provider: WallpaperProviderInterface,
    store: PersonalizationStore): Promise<void> {
  store.dispatch(action.beginUpdateDailyRefreshImageAction());
  store.dispatch(action.beginLoadSelectedImageAction());
  const {success} = await provider.updateDailyRefreshWallpaper();
  if (success) {
    store.dispatch(action.setUpdatedDailyRefreshImageAction());
  }
}

/** Confirm and set preview wallpaper as actual wallpaper. */
export async function confirmPreviewWallpaper(
    provider: WallpaperProviderInterface): Promise<void> {
  await provider.confirmPreviewWallpaper();
  provider.makeOpaque();
}

/** Cancel preview wallpaper and show the previous wallpaper. */
export async function cancelPreviewWallpaper(
    provider: WallpaperProviderInterface): Promise<void> {
  await provider.cancelPreviewWallpaper();
  provider.makeOpaque();
}

/**
 * Fetches list of collections, then fetches list of images for each
 * collection.
 */
export async function initializeBackdropData(
    provider: WallpaperProviderInterface,
    store: PersonalizationStore): Promise<void> {
  await fetchCollections(provider, store);
  await fetchAllImagesForCollections(provider, store);
}

/** Fetches initial Google Photos data and saves it to the store. */
export async function initializeGooglePhotosData(
    provider: WallpaperProviderInterface,
    store: PersonalizationStore): Promise<void> {
  // Fetch whether the user is allowed to access Google Photos.
  await fetchGooglePhotosEnabled(provider, store);

  // Only proceed to fetch Google Photos data if the user is allowed.
  const enabled = store.data.wallpaper.googlePhotos.enabled;
  if (enabled === GooglePhotosEnablementState.kEnabled) {
    await fetchGooglePhotosCount(provider, store);
  } else {
    store.beginBatchUpdate();
    store.dispatch(action.beginLoadGooglePhotosCountAction());
    store.dispatch(action.setGooglePhotosCountAction(null));
    store.endBatchUpdate();
  }

  // If the count of Google Photos photos is zero or null, it's not necesssary
  // to query the server for the list of albums/photos.
  const count = store.data.wallpaper.googlePhotos.count;
  if (count === 0 || count === null) {
    const result = count === 0 ? [] : null;
    const resumeToken = null;
    store.beginBatchUpdate();
    store.dispatch(action.beginLoadGooglePhotosAlbumsAction());
    store.dispatch(action.beginLoadGooglePhotosPhotosAction());
    store.dispatch(action.appendGooglePhotosAlbumsAction(result, resumeToken));
    store.dispatch(action.appendGooglePhotosPhotosAction(result, resumeToken));
    store.endBatchUpdate();
    return;
  }

  await Promise.all([
    fetchGooglePhotosAlbums(provider, store),
    fetchGooglePhotosPhotos(provider, store),
  ]);
}

/**
 * Gets list of local images, then fetches image thumbnails for each local
 * image.
 */
export async function fetchLocalData(
    provider: WallpaperProviderInterface,
    store: PersonalizationStore): Promise<void> {
  // Do not restart loading local image list if a load is already in progress.
  if (!store.data.wallpaper.loading.local.images) {
    await getLocalImages(provider, store);
  }
  await getMissingLocalImageThumbnails(provider, store);
}
