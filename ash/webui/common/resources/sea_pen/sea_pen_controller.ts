// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';

import {MantaStatusCode, SeaPenFeedbackMetadata, SeaPenProviderInterface, SeaPenQuery, SeaPenThumbnail} from './sea_pen.mojom-webui.js';
import * as seaPenAction from './sea_pen_actions.js';
import {SeaPenStoreInterface} from './sea_pen_store.js';
import {isNonEmptyArray, isNonEmptyFilePath} from './sea_pen_utils.js';

export async function selectRecentSeaPenImage(
    image: FilePath, provider: SeaPenProviderInterface,
    store: SeaPenStoreInterface): Promise<void> {
  // Returns if the selected image is the current wallpaper.
  if (isNonEmptyFilePath(image) && image.path === store.data.currentSelected) {
    return;
  }
  // Batch these changes together to reduce polymer churn as multiple state
  // fields change quickly.
  store.beginBatchUpdate();
  store.dispatch(seaPenAction.beginSelectRecentSeaPenImageAction(image));
  store.dispatch(seaPenAction.beginLoadSelectedRecentSeaPenImageAction());
  store.endBatchUpdate();

  const {success} = await provider.selectRecentSeaPenImage(image);

  store.beginBatchUpdate();
  store.dispatch(seaPenAction.endSelectRecentSeaPenImageAction(image, success));
  if (!success) {
    console.warn('Error setting wallpaper');
  }
  store.endBatchUpdate();
}

export async function searchSeaPenThumbnails(
    query: SeaPenQuery, provider: SeaPenProviderInterface,
    store: SeaPenStoreInterface): Promise<void> {
  store.dispatch(seaPenAction.beginSearchSeaPenThumbnailsAction(query));
  const {images, statusCode} = await provider.searchWallpaper(query);
  if (!isNonEmptyArray(images) || statusCode !== MantaStatusCode.kOk) {
    console.warn('Error generating thumbnails. Status code: ', statusCode);
  }
  store.dispatch(seaPenAction.setThumbnailResponseStatusCodeAction(statusCode));
  store.dispatch(seaPenAction.setSeaPenThumbnailsAction(query, images));
}

export async function selectSeaPenWallpaper(
    thumbnail: SeaPenThumbnail, provider: SeaPenProviderInterface,
    store: SeaPenStoreInterface): Promise<void> {
  store.dispatch(seaPenAction.beginSelectSeaPenThumbnailAction(thumbnail));
  const {success} = await provider.selectSeaPenThumbnail(thumbnail.id);
  store.dispatch(
      seaPenAction.endSelectSeaPenThumbnailAction(thumbnail, success));
  if (store.data.loading.setImage === 0) {
    // If the user has not already clicked on another thumbnail, treat this
    // thumbnail as set.
    // TODO(b/321252838) improve this with an async observer for VC Background.
    store.dispatch(seaPenAction.setSelectedRecentSeaPenImageAction(
        success ? `${thumbnail.id}.jpg` : null));
  }
  // Re-fetches the recent Sea Pen image if setting sea pen wallpaper
  // successfully, which means the file has been downloaded successfully.
  if (success) {
    await fetchRecentSeaPenData(provider, store);
  }
}

export async function clearSeaPenThumbnails(store: SeaPenStoreInterface) {
  store.dispatch(seaPenAction.clearSeaPenThumbnailsAction());
}

export async function deleteRecentSeaPenImage(
    image: FilePath, provider: SeaPenProviderInterface,
    store: SeaPenStoreInterface): Promise<void> {
  const {success} = await provider.deleteRecentSeaPenImage(image);
  // Re-fetches the recent Sea Pen images if recent Sea Pen image is removed
  // successfully.
  if (success) {
    fetchRecentSeaPenData(provider, store);
  }
}

export async function getRecentSeaPenImages(
    provider: SeaPenProviderInterface,
    store: SeaPenStoreInterface): Promise<void> {
  store.dispatch(seaPenAction.beginLoadRecentSeaPenImagesAction());

  const {images} = await provider.getRecentSeaPenImages();
  if (images == null) {
    console.warn('Failed to fetch recent sea pen images');
  }

  store.dispatch(seaPenAction.setRecentSeaPenImagesAction(images));
}

/**
 * Gets list of recent Sea Pen images, then fetches image data for each recent
 * Sea Pen image.
 */
export async function fetchRecentSeaPenData(
    provider: SeaPenProviderInterface,
    store: SeaPenStoreInterface): Promise<void> {
  // Do not restart loading local image list if a load is already in progress.
  if (!store.data.loading.recentImages) {
    await getRecentSeaPenImages(provider, store);
  }
  await getMissingRecentSeaPenImageData(provider, store);
}

/**
 * Because data loading can happen asynchronously and is triggered
 * on page load and on window focus, multiple "threads" can be fetching
 * data simultaneously. Synchronize them with a task queue.
 */
const recentSeaPenImageDataToFetch = new Set<FilePath['path']>();

/**
 * Get an sea pen data one at a time for every recent Sea Pen image that does
 * not have the data yet.
 */
async function getMissingRecentSeaPenImageData(
    provider: SeaPenProviderInterface,
    store: SeaPenStoreInterface): Promise<void> {
  if (!Array.isArray(store.data.recentImages)) {
    console.warn('Cannot fetch thumbnails with invalid image list');
    return;
  }
  // Set correct loading state for each image thumbnail. Do in a batch update to
  // reduce number of times that polymer must re-render.
  store.beginBatchUpdate();
  for (const image of store.data.recentImages) {
    if (store.data.recentImageData[image.path] ||
        store.data.loading.recentImageData[image.path] ||
        recentSeaPenImageDataToFetch.has(image.path)) {
      // Do not re-load thumbnail if already present, or already loading.
      continue;
    }
    recentSeaPenImageDataToFetch.add(image.path);
    store.dispatch(seaPenAction.beginLoadRecentSeaPenImageDataAction(image));
  }
  store.endBatchUpdate();

  // There may be multiple async tasks triggered that pull off this queue.
  while (recentSeaPenImageDataToFetch.size) {
    await Promise.all(
        Array.from(recentSeaPenImageDataToFetch).map(async path => {
          recentSeaPenImageDataToFetch.delete(path);
          const {url} = await provider.getRecentSeaPenImageThumbnail({path});
          // TODO(b/312783231): add real API to get the image query info.
          const queryInfo =
              'query ' + Math.floor(Math.random() * 100 + 1).toString();
          if (!url) {
            console.warn('Failed to fetch recent Sea Pen image data', path);
          }
          store.dispatch(seaPenAction.setRecentSeaPenImageDataAction(
              {path}, {url, queryInfo}));
        }));
  }
}

export function openFeedbackDialog(
    metadata: SeaPenFeedbackMetadata, provider: SeaPenProviderInterface) {
  provider.openFeedbackDialog(metadata);
}

export async function getShouldShowSeaPenTermsOfServiceDialog(
    provider: SeaPenProviderInterface,
    store: SeaPenStoreInterface): Promise<void> {
  const {shouldShowDialog} =
      await provider.shouldShowSeaPenTermsOfServiceDialog();

  // Dispatch action to set the should show dialog boolean.
  store.dispatch(seaPenAction.setShouldShowSeaPenTermsOfServiceDialogAction(
      shouldShowDialog));
}

export async function acceptSeaPenTermsOfService(
    provider: SeaPenProviderInterface,
    store: SeaPenStoreInterface): Promise<void> {
  if (!store.data.shouldShowSeaPenTermsOfServiceDialog) {
    // Do nothing if the terms are already accepted;
    return;
  }

  await provider.handleSeaPenTermsOfServiceAccepted();

  // Dispatch action to set the should show dialog boolean.
  store.dispatch(
      seaPenAction.setShouldShowSeaPenTermsOfServiceDialogAction(false));
}
