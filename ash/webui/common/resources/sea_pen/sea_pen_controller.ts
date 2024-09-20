// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FullscreenPreviewState} from 'chrome://resources/ash/common/personalization/wallpaper_state.js';

import {QUERY, SeaPenImageId} from './constants.js';
import {isSeaPenTextInputEnabled} from './load_time_booleans.js';
import {MantaStatusCode, SeaPenFeedbackMetadata, SeaPenProviderInterface, SeaPenQuery, SeaPenThumbnail} from './sea_pen.mojom-webui.js';
import * as seaPenAction from './sea_pen_actions.js';
import {logSeaPenImageSet} from './sea_pen_metrics_logger.js';
import {SeaPenStoreInterface} from './sea_pen_store.js';
import {isNonEmptyArray, isPersonalizationApp} from './sea_pen_utils.js';
import {withMinimumDelay} from './transition.js';

export async function selectRecentSeaPenImage(
    id: SeaPenImageId, provider: SeaPenProviderInterface,
    store: SeaPenStoreInterface): Promise<void> {
  if (id === store.data.currentSelected) {
    // Return if the just selected image is already the current image.
    return;
  }
  // Batch these changes together to reduce polymer churn as multiple state
  // fields change quickly.
  store.beginBatchUpdate();
  store.dispatch(seaPenAction.beginSelectRecentSeaPenImageAction(id));
  store.dispatch(seaPenAction.beginLoadSelectedRecentSeaPenImageAction());
  store.endBatchUpdate();

  const shouldPreview = await shouldShowFullscreenPreview(provider);
  if (shouldPreview) {
    provider.makeTransparent();
    store.dispatch(seaPenAction.setSeaPenFullscreenStateAction(
        FullscreenPreviewState.LOADING));
  }
  const {success} = await provider.selectRecentSeaPenImage(id, shouldPreview);

  store.beginBatchUpdate();
  store.dispatch(seaPenAction.endSelectRecentSeaPenImageAction(id, success));
  if (!success) {
    console.warn('Error setting image');
    store.dispatch(seaPenAction.setSeaPenFullscreenStateAction(
        FullscreenPreviewState.OFF));
    // Revert back to the old one.
    store.dispatch(seaPenAction.setSelectedRecentSeaPenImageAction(
        store.data.currentSelected));
  }
  store.endBatchUpdate();

  if (success) {
    const isTextQuery =
        !!store.data.recentImageData[id]?.imageInfo?.query?.textQuery;
    logSeaPenImageSet(isTextQuery, /*source=*/ 'Recent');
  }
}

export async function getSeaPenThumbnails(
    query: SeaPenQuery, provider: SeaPenProviderInterface,
    store: SeaPenStoreInterface): Promise<void> {
  store.dispatch(seaPenAction.beginSearchSeaPenThumbnailsAction(query));
  store.dispatch(seaPenAction.setCurrentSeaPenQueryAction(query));
  const {thumbnails, statusCode} =
      await withMinimumDelay(provider.getSeaPenThumbnails(query));
  if (!isNonEmptyArray(thumbnails) || statusCode !== MantaStatusCode.kOk) {
    console.warn('Error generating thumbnails. Status code: ', statusCode);
  }

  // New requests might have been made for a different template so we should
  // only return results for the request that matches the template that the user
  // is using or when the user is not viewing the template to clear the loading
  // UI.
  // TODO(b/333924681): Implement a better way to handle the race condition.
  // The current logic does not handle the case the user lands on the original
  // template after navigating between different templates.
  const params = new URLSearchParams(window.location.search);
  const templateIdParam = params.get('seaPenTemplateId');
  if (!templateIdParam ||
      (templateIdParam === query.templateQuery?.id.toString()) ||
      (templateIdParam === QUERY && !!query.textQuery)) {
    store.dispatch(
        seaPenAction.setThumbnailResponseStatusCodeAction(statusCode));
    store.dispatch(seaPenAction.setSeaPenThumbnailsAction(query, thumbnails));
  }
}

export async function selectSeaPenThumbnail(
    thumbnail: SeaPenThumbnail, provider: SeaPenProviderInterface,
    store: SeaPenStoreInterface): Promise<void> {
  if (store.data.recentImages &&
      store.data.recentImages.includes(thumbnail.id)) {
    return selectRecentSeaPenImage(thumbnail.id, provider, store);
  }

  let promise: ReturnType<SeaPenProviderInterface['selectSeaPenThumbnail']>;

  store.dispatch(seaPenAction.beginSelectSeaPenThumbnailAction(thumbnail));

  const shouldPreview = await shouldShowFullscreenPreview(provider);
  if (shouldPreview) {
    provider.makeTransparent();
    store.dispatch(seaPenAction.setSeaPenFullscreenStateAction(
        FullscreenPreviewState.LOADING));
  }
  if (isPersonalizationApp()) {
    promise = withMinimumDelay(
        provider.selectSeaPenThumbnail(thumbnail.id, shouldPreview));
  } else {
    // VC Background should not start the visual loading state immediately. The
    // async request will resolve very quickly.
    store.beginBatchUpdate();
    promise = provider.selectSeaPenThumbnail(thumbnail.id, shouldPreview);
  }

  const {success} = await promise;

  store.beginBatchUpdate();
  store.dispatch(
      seaPenAction.endSelectSeaPenThumbnailAction(thumbnail, success));

  if (!success) {
    store.dispatch(seaPenAction.setSeaPenFullscreenStateAction(
        FullscreenPreviewState.OFF));
    // Revert back to the original one.
    store.dispatch(seaPenAction.setSelectedRecentSeaPenImageAction(
        store.data.currentSelected));
  }
  store.endBatchUpdate();
  // Re-fetches the recent SeaPen image if setting SeaPen thumbnail
  // successfully, which means the file has been downloaded successfully.
  if (success) {
    const isTextQuery = !!store.data.currentSeaPenQuery?.textQuery;
    logSeaPenImageSet(isTextQuery, /*source=*/ 'Create');
    await fetchRecentSeaPenData(provider, store);
  }
}

export async function clearSeaPenThumbnails(store: SeaPenStoreInterface) {
  store.dispatch(seaPenAction.clearSeaPenThumbnailsAction());
}

export async function cleanUpSeaPenQueryStates(store: SeaPenStoreInterface) {
  store.beginBatchUpdate();
  store.dispatch(seaPenAction.setThumbnailResponseStatusCodeAction(null));
  store.dispatch(seaPenAction.clearCurrentSeaPenQueryAction());
  store.dispatch(seaPenAction.clearSeaPenThumbnailsLoadingAction());
  store.endBatchUpdate();
}

export async function deleteRecentSeaPenImage(
    id: SeaPenImageId, provider: SeaPenProviderInterface,
    store: SeaPenStoreInterface): Promise<void> {
  const {success} = await provider.deleteRecentSeaPenImage(id);
  // Re-fetches the recent Sea Pen images if recent Sea Pen image is removed
  // successfully.
  if (success) {
    fetchRecentSeaPenData(provider, store);
  }
}

export async function getRecentSeaPenImageIds(
    provider: SeaPenProviderInterface,
    store: SeaPenStoreInterface): Promise<void> {
  store.dispatch(seaPenAction.beginLoadRecentSeaPenImagesAction());

  const {ids} = await provider.getRecentSeaPenImageIds();
  if (ids == null) {
    console.warn('Failed to fetch recent sea pen images');
  }

  store.dispatch(seaPenAction.setRecentSeaPenImagesAction(ids));
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
    await getRecentSeaPenImageIds(provider, store);
  }
  await getMissingRecentSeaPenImageData(provider, store);
}

/**
 * Because data loading can happen asynchronously and is triggered
 * on page load and on window focus, multiple "threads" can be fetching
 * data simultaneously. Synchronize them with a task queue.
 */
const recentSeaPenImageDataToFetch = new Set<SeaPenImageId>();

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
  for (const id of store.data.recentImages) {
    if (store.data.recentImageData[id] ||
        store.data.loading.recentImageData[id] ||
        recentSeaPenImageDataToFetch.has(id)) {
      // Do not re-load thumbnail if already present, or already loading.
      continue;
    }
    recentSeaPenImageDataToFetch.add(id);
    store.dispatch(seaPenAction.beginLoadRecentSeaPenImageDataAction(id));
  }
  store.endBatchUpdate();

  // There may be multiple async tasks triggered that pull off this queue.
  while (recentSeaPenImageDataToFetch.size) {
    await Promise.all(Array.from(recentSeaPenImageDataToFetch).map(async id => {
      recentSeaPenImageDataToFetch.delete(id);
      const {thumbnailData} = await provider.getRecentSeaPenImageThumbnail(id);
      if (!thumbnailData) {
        console.warn('Failed to fetch recent Sea Pen image data', id);
      }
      store.dispatch(
          seaPenAction.setRecentSeaPenImageDataAction(id, thumbnailData));
    }));
  }
}

export function openFeedbackDialog(
    metadata: SeaPenFeedbackMetadata, provider: SeaPenProviderInterface) {
  provider.openFeedbackDialog(metadata);
}

export async function getShouldShowSeaPenIntroductionDialog(
    provider: SeaPenProviderInterface,
    store: SeaPenStoreInterface): Promise<void> {
  const {shouldShowDialog} =
      await provider.shouldShowSeaPenIntroductionDialog();

  // Dispatch action to set the should show dialog boolean.
  store.dispatch(seaPenAction.setShouldShowSeaPenIntroductionDialogAction(
      shouldShowDialog));
}

export async function closeSeaPenIntroductionDialog(
    provider: SeaPenProviderInterface,
    store: SeaPenStoreInterface): Promise<void> {
  if (!store.data.shouldShowSeaPenIntroductionDialog) {
    // Do nothing if the introduction dialog is already closed;
    return;
  }

  await provider.handleSeaPenIntroductionDialogClosed();

  // Dispatch action to set the should show dialog boolean.
  store.dispatch(
      seaPenAction.setShouldShowSeaPenIntroductionDialogAction(false));
}

/**
 * Check whether to show fullscreen preview while selecting a SeaPen image
 * as wallpaper.
 */
async function shouldShowFullscreenPreview(provider: SeaPenProviderInterface):
    Promise<boolean> {
  if (!isPersonalizationApp() || !isSeaPenTextInputEnabled()) {
    return false;
  }
  const {tabletMode} = await provider.isInTabletMode();
  return tabletMode;
}
