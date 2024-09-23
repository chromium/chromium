// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FullscreenPreviewState} from 'chrome://resources/ash/common/personalization/wallpaper_state.js';
import {isNonEmptyArray, isNonEmptyFilePath} from 'chrome://resources/ash/common/sea_pen/sea_pen_utils.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {CurrentWallpaper, GooglePhotosAlbum, GooglePhotosEnablementState, GooglePhotosPhoto, WallpaperCollection, WallpaperImage, WallpaperLayout, WallpaperProviderInterface, WallpaperType} from '../../personalization_app.mojom-webui.js';
import {setErrorAction} from '../personalization_actions.js';
import {PersonalizationStore} from '../personalization_store.js';

import {DisplayableImage} from './constants.js';
import {isDefaultImage, isGooglePhotosPhoto, isImageAMatchForKey, isImageEqualToSelected, isWallpaperImage} from './utils.js';
import * as action from './wallpaper_actions.js';
import {DailyRefreshType} from './wallpaper_state.js';

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
 * Appends a suffix to request wallpaper images with the longest of width or
 * height being 512 pixels. This should ensure that the wallpaper image is
 * large enough to cover a grid item but not significantly more so.
 */
function appendMaxResolutionSuffix(value: Url): Url {
  return {...value, url: value.url + '=s512'};
}

/**
 * Fetches the list of Google Photos photos for the album associated with the
 * specified id and saves it to the store.
 */
export async function fetchGooglePhotosAlbum(
    provider: WallpaperProviderInterface, store: PersonalizationStore,
    albumId: string): Promise<void> {
  // Photos should only be fetched after determining whether access is allowed.
  const enabled = store.data.wallpaper.googlePhotos.enabled;
  assert(enabled !== undefined);

  store.dispatch(action.beginLoadGooglePhotosAlbumAction(albumId));

  // If access is *not* allowed, short-circuit the request.
  if (enabled !== GooglePhotosEnablementState.kEnabled) {
    store.dispatch(action.appendGooglePhotosPhotosAction(
        /*photos=*/ null, /*resumeToken=*/ null));
    return;
  }

  let photos: GooglePhotosPhoto[]|null = [];
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

/** Fetches the list of Google Photos owned albums and saves it to the store. */
export async function fetchGooglePhotosAlbums(
    provider: WallpaperProviderInterface,
    store: PersonalizationStore): Promise<void> {
  // Albums should only be fetched after determining whether access is allowed.
  const enabled = store.data.wallpaper.googlePhotos.enabled;
  assert(enabled !== undefined, 'Google Photos albums not enabled.');

  store.dispatch(action.beginLoadGooglePhotosAlbumsAction());

  // If access is *not* allowed, short-circuit the request.
  if (enabled !== GooglePhotosEnablementState.kEnabled) {
    store.dispatch(action.appendGooglePhotosAlbumsAction(
        /*albums=*/ null, /*resumeToken=*/ null));
    return;
  }

  let albums: GooglePhotosAlbum[]|null = [];
  let resumeToken = store.data.wallpaper.googlePhotos.resumeTokens.albums;

  const {response} = await provider.fetchGooglePhotosAlbums(resumeToken);
  if (Array.isArray(response.albums)) {
    albums.push(...response.albums);
    resumeToken = response.resumeToken || null;
  } else {
    console.warn('Failed to fetch Google Photos owned albums');
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

/**
 * Fetches the list of Google Photos shared albums and saves it to the store.
 */
export async function fetchGooglePhotosSharedAlbums(
    provider: WallpaperProviderInterface,
    store: PersonalizationStore): Promise<void> {
  // Albums should only be fetched after determining whether access is allowed.
  const enabled = store.data.wallpaper.googlePhotos.enabled;
  assert(
      enabled !== undefined, 'Google photos enablement state not initialized.');

  store.dispatch(action.beginLoadGooglePhotosSharedAlbumsAction());

  // If access is *not* allowed, short-circuit the request.
  if (enabled !== GooglePhotosEnablementState.kEnabled) {
    store.dispatch(action.appendGooglePhotosSharedAlbumsAction(
        /*albums=*/ null, /*resumeToken=*/ null));
    return;
  }

  let albums: GooglePhotosAlbum[]|null = [];
  let resumeToken = store.data.wallpaper.googlePhotos.resumeTokens.albumsShared;

  const {response} = await provider.fetchGooglePhotosSharedAlbums(resumeToken);
  if (Array.isArray(response.albums)) {
    albums.push(...response.albums);
    resumeToken = response.resumeToken || null;
  } else {
    console.warn('Failed to fetch Google Photos shared albums');
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

  store.dispatch(
      action.appendGooglePhotosSharedAlbumsAction(albums, resumeToken));
}

/** Fetches whether the user is allowed to access Google Photos. */
export async function fetchGooglePhotosEnabled(
    provider: WallpaperProviderInterface,
    store: PersonalizationStore): Promise<void> {
  // Whether access is allowed should only be fetched once.
  if (store.data.wallpaper.googlePhotos.enabled !== undefined) {
    return;
  }

  store.dispatch(action.beginLoadGooglePhotosEnabledAction());
  const {state} = await provider.fetchGooglePhotosEnabled();
  if (state === GooglePhotosEnablementState.kError) {
    console.warn('Failed to fetch Google Photos enabled');
  }
  store.dispatch(action.setGooglePhotosEnabledAction(state));
}

/** Fetches the list of Google Photos photos and saves it to the store. */
export async function fetchGooglePhotosPhotos(
    provider: WallpaperProviderInterface,
    store: PersonalizationStore): Promise<void> {
  // Photos should only be fetched after determining whether access is allowed.
  const enabled = store.data.wallpaper.googlePhotos.enabled;
  assert(enabled !== undefined);

  store.dispatch(action.beginLoadGooglePhotosPhotosAction());

  // If access is *not* allowed, short-circuit the request.
  if (enabled !== GooglePhotosEnablementState.kEnabled) {
    store.dispatch(action.appendGooglePhotosPhotosAction(
        /*photos=*/ null, /*resumeToken=*/ null));
    return;
  }

  let photos: GooglePhotosPhoto[]|null = [];
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

export async function getDefaultImageThumbnail(
    provider: WallpaperProviderInterface,
    store: PersonalizationStore): Promise<void> {
  store.dispatch(action.beginLoadDefaultImageThubmnailAction());
  const {data} = await provider.getDefaultImageThumbnail();
  if (data.url.length === 0) {
    console.error('Failed to load default image thumbnail');
  }
  store.dispatch(action.setDefaultImageThumbnailAction(data));
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
  for (const image of store.data.wallpaper.local.images) {
    if (isDefaultImage(image)) {
      continue;
    }
    if (store.data.wallpaper.local.data[image.path] ||
        store.data.wallpaper.loading.local.data[image.path] ||
        imageThumbnailsToFetch.has(image.path)) {
      // Do not re-load thumbnail if already present, or already loading.
      continue;
    }
    imageThumbnailsToFetch.add(image.path);
    store.dispatch(action.beginLoadLocalImageDataAction(image));
  }
  store.endBatchUpdate();

  // There may be multiple async tasks triggered that pull off this queue.
  while (imageThumbnailsToFetch.size) {
    await Promise.all(Array.from(imageThumbnailsToFetch).map(async path => {
      imageThumbnailsToFetch.delete(path);
      const {data} = await provider.getLocalImageThumbnail({path});
      if (!data) {
        console.warn('Failed to fetch local image data', path);
      }
      store.dispatch(action.setLocalImageDataAction({path}, data));
    }));
  }
}

export async function selectWallpaper(
    image: DisplayableImage, provider: WallpaperProviderInterface,
    store: PersonalizationStore,
    layout: WallpaperLayout = WallpaperLayout.kCenterCropped): Promise<void> {
  const currentWallpaper = store.data.wallpaper.currentSelected;
  if (currentWallpaper && isImageEqualToSelected(image, currentWallpaper)) {
    return;
  }

  // Batch these changes together to reduce polymer churn as multiple state
  // fields change quickly.
  store.beginBatchUpdate();
  store.dispatch(action.beginSelectImageAction(image));
  store.dispatch(action.beginLoadSelectedImageAction());
  const {tabletMode} = await provider.isInTabletMode();
  const shouldPreview = tabletMode && !isDefaultImage(image);
  if (shouldPreview) {
    provider.makeTransparent();
    store.dispatch(
        action.setFullscreenStateAction(FullscreenPreviewState.LOADING));
  }
  store.endBatchUpdate();
  const {success} = await (() => {
    if (isWallpaperImage(image)) {
      return provider.selectWallpaper(
          image.unitId, /*preview_mode=*/ shouldPreview);
    } else if (isDefaultImage(image)) {
      return provider.selectDefaultImage();
    } else if (isNonEmptyFilePath(image)) {
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
  if (!success) {
    console.warn('Error setting wallpaper');
    store.dispatch(action.setFullscreenStateAction(FullscreenPreviewState.OFF));
    store.dispatch(
        action.setAttributionAction(store.data.wallpaper.attribution));
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
      image.type === WallpaperType.kOnceGooglePhotos);
  assert(
      layout === WallpaperLayout.kCenter ||
      layout === WallpaperLayout.kCenterCropped);

  if (image.layout === layout) {
    return;
  }

  store.dispatch(action.beginLoadSelectedImageAction());
  await provider.setCurrentWallpaperLayout(layout);
}

// Do not trigger the loading UI if the currently selected wallpaper is a
// matching type for the incoming selection and if the currently selected
// wallpaper is in the chosen album.
function dailyRefreshShouldTriggerLoading(
    id: string, types: Set<WallpaperType>,
    currentSelected: CurrentWallpaper|null,
    imagesById:
        Record<string, Array<WallpaperImage|GooglePhotosPhoto>|null|undefined>):
    boolean {
  if (!id) {
    // No loading shown if clearing daily refresh state.
    return false;
  }
  if (!currentSelected) {
    return true;
  }
  if (types.has(currentSelected.type)) {
    return !imagesById[id]?.some(
        image => isImageAMatchForKey(image, currentSelected.key));
  }
  return true;
}

export async function setDailyRefreshCollectionId(
    collectionId: WallpaperCollection['id'],
    provider: WallpaperProviderInterface,
    store: PersonalizationStore): Promise<void> {
  if (dailyRefreshShouldTriggerLoading(
          collectionId, new Set([WallpaperType.kOnline, WallpaperType.kDaily]),
          store.data.wallpaper.currentSelected,
          store.data.wallpaper.backdrop.images)) {
    store.dispatch(action.beginUpdateDailyRefreshImageAction());
  }
  const {success} = await provider.setDailyRefreshCollectionId(collectionId);
  if (!success) {
    store.dispatch(
        setErrorAction({message: loadTimeData.getString('setWallpaperError')}));
  }
  await getDailyRefreshState(provider, store);
}

export async function selectGooglePhotosAlbum(
    albumId: GooglePhotosAlbum['id'], provider: WallpaperProviderInterface,
    store: PersonalizationStore): Promise<void> {
  if (dailyRefreshShouldTriggerLoading(
          albumId, new Set([
            WallpaperType.kOnceGooglePhotos,
            WallpaperType.kDailyGooglePhotos,
          ]),
          store.data.wallpaper.currentSelected,
          store.data.wallpaper.googlePhotos.photosByAlbumId)) {
    store.dispatch(action.beginUpdateDailyRefreshImageAction());
  }
  const {success} = await provider.selectGooglePhotosAlbum(albumId);
  if (!success) {
    store.dispatch(
        setErrorAction({message: loadTimeData.getString('googlePhotosError')}));
  }
  await getDailyRefreshState(provider, store);
}

/**
 * Get the currently active daily refresh id for Backdrop and Google Photos.
 * One or both will be empty, depending on which, if either, is enabled.
 */
export async function getDailyRefreshState(
    provider: WallpaperProviderInterface,
    store: PersonalizationStore): Promise<void> {
  const [{collectionId}, {albumId}] = await Promise.all([
    provider.getDailyRefreshCollectionId(),
    provider.getGooglePhotosDailyRefreshAlbumId(),
  ]);

  // Daily refresh should only be active for either Backdrop or Google Photos
  assert(!collectionId || !albumId);

  if (collectionId) {
    store.dispatch(action.setDailyRefreshCollectionIdAction(collectionId));
  } else if (albumId) {
    store.dispatch(action.setGooglePhotosDailyRefreshAlbumIdAction(albumId));
  } else {
    store.dispatch(action.clearDailyRefreshAction());
  }
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
  } else {
    const currentAttribution = store.data.wallpaper.attribution;
    const currentWallpaper = store.data.wallpaper.currentSelected;
    const dailyRefresh = store.data.wallpaper.dailyRefresh;
    // Displays error if daily refresh is activated for Google Photos album
    // and refresh failed to fetch a new Google Photo wallpaper.
    // Also dispatches setUpdatedDailyRefreshImageAction() and
    // setSelectedImageAction() to avoid pending UI.
    // TODO (b/266257678): displays error message when daily refresh fails for
    // online wallpaper collections.
    if (!!dailyRefresh && dailyRefresh.type == DailyRefreshType.GOOGLE_PHOTOS) {
      store.dispatch(action.setUpdatedDailyRefreshImageAction());
      store.dispatch(action.setAttributionAction(currentAttribution));
      store.dispatch(action.setSelectedImageAction(currentWallpaper));
      store.dispatch(setErrorAction(
          {message: loadTimeData.getString('googlePhotosError')}));
    }
  }
}

/** Confirm and set preview wallpaper as actual wallpaper. */
export async function confirmPreviewWallpaper(
    provider: WallpaperProviderInterface): Promise<void> {
  provider.makeOpaque();
  provider.confirmPreviewWallpaper();
}

/** Cancel preview wallpaper and show the previous wallpaper. */
export async function cancelPreviewWallpaper(
    provider: WallpaperProviderInterface): Promise<void> {
  provider.makeOpaque();
  provider.cancelPreviewWallpaper();
}

export async function getShouldShowTimeOfDayWallpaperDialog(
    provider: WallpaperProviderInterface, store: PersonalizationStore) {
  const {shouldShowDialog} =
      await provider.shouldShowTimeOfDayWallpaperDialog();

  // Dispatch action to set the should show dialog boolean.
  store.dispatch(
      action.setShouldShowTimeOfDayWallpaperDialog(shouldShowDialog));
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
