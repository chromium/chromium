// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';

import {SeaPenProviderInterface, SeaPenQuery, SeaPenThumbnail} from '../../../sea_pen.mojom-webui.js';
import {PersonalizationStore} from '../../personalization_store.js';
import {isNonEmptyArray} from '../../utils.js';
import {isImageEqualToSelected} from '../utils.js';
import * as action from '../wallpaper_actions.js';

import * as seaPenAction from './sea_pen_actions.js';

export async function selectRecentSeaPenImage(
    image: FilePath, provider: SeaPenProviderInterface,
    store: PersonalizationStore): Promise<void> {
  const currentWallpaper = store.data.wallpaper.currentSelected;
  if (currentWallpaper && isImageEqualToSelected(image, currentWallpaper)) {
    return;
  }
  // Batch these changes together to reduce polymer churn as multiple state
  // fields change quickly.
  store.beginBatchUpdate();
  store.dispatch(action.beginSelectImageAction(image));
  store.dispatch(action.beginLoadSelectedImageAction());
  store.endBatchUpdate();

  const {success} = await provider.selectRecentSeaPenImage(image);

  store.beginBatchUpdate();
  store.dispatch(action.endSelectImageAction(image, success));
  if (!success) {
    console.warn('Error setting wallpaper');
    store.dispatch(
        action.setAttributionAction(store.data.wallpaper.attribution));
    store.dispatch(
        action.setSelectedImageAction(store.data.wallpaper.currentSelected));
  }
  store.endBatchUpdate();
}

export async function searchSeaPenThumbnails(
    query: SeaPenQuery, provider: SeaPenProviderInterface,
    store: PersonalizationStore): Promise<void> {
  store.dispatch(seaPenAction.beginSearchSeaPenThumbnailsAction(query));
  const {images} = await provider.searchWallpaper(query);
  if (!isNonEmptyArray(images)) {
    console.warn('Failed to generate thumbnails.');
  }
  store.dispatch(seaPenAction.setSeaPenThumbnailsAction(query, images));
}

export async function selectSeaPenWallpaper(
    thumbnail: SeaPenThumbnail,
    provider: SeaPenProviderInterface): Promise<void> {
  // TODO(b/305965517) show loading state.
  await provider.selectSeaPenThumbnail(thumbnail.id);
}

export async function getRecentSeaPenImages(
    provider: SeaPenProviderInterface,
    store: PersonalizationStore): Promise<void> {
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
    store: PersonalizationStore): Promise<void> {
  // Do not restart loading local image list if a load is already in progress.
  if (!store.data.wallpaper.loading.seaPen.recentImages) {
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
    store: PersonalizationStore): Promise<void> {
  if (!Array.isArray(store.data.wallpaper.seaPen.recentImages)) {
    console.warn('Cannot fetch thumbnails with invalid image list');
    return;
  }
  // Set correct loading state for each image thumbnail. Do in a batch update to
  // reduce number of times that polymer must re-render.
  store.beginBatchUpdate();
  for (const image of store.data.wallpaper.seaPen.recentImages) {
    if (store.data.wallpaper.seaPen.recentImageData[image.path] ||
        store.data.wallpaper.loading.seaPen.recentImageData[image.path] ||
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
