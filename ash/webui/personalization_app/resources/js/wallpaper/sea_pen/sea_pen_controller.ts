// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SeaPenProviderInterface, SeaPenQuery, SeaPenThumbnail} from '../../../sea_pen.mojom-webui.js';
import {PersonalizationStore} from '../../personalization_store.js';
import {isNonEmptyArray} from '../../utils.js';
import {SeaPenWallpaper} from '../constants.js';
import {isImageEqualToSelected} from '../utils.js';
import * as action from '../wallpaper_actions.js';

import * as seaPenAction from './sea_pen_actions.js';

export async function getRecentSeaPenImages(store: PersonalizationStore):
    Promise<void> {
  // TODO(b/304576846): remove the function, use the real api to get the images
  // and dispatch the action in sea pen observer.
  const images = [
    {
      query_info: 'a close up of a flower with water drops on it',
      url: {
        url:
            'https://lh5.googleusercontent.com/proxy/POggSGKiyt380V63sTRjua4Q6s6v02wNfTyeDhTK1TKjlZrEnRiZNHa4lDSXu_3mvdUGQe2HF0s_Z8J45ygrJ3jM9R6bZUcF-CN61iacGXrOVWr6YdbaDwuhZu7N2RxJRMKT2Wnrifc',
      },
      file_path: {path: '/sea_pen/111.jpg'},
    },
    {
      query_info:
          'a large white ball in the middle of a field with soap bubbles',
      url: {
        url:
            'https://lh4.googleusercontent.com/proxy/yRB8hlnV86jWE3XgtAOd2Hniso9cv5YynGEBQrnVr26onWSvNWARKahdFxiSgv5CKVDnpgZ4LunQ7cxTX5ZGf4nZNVHjQ88xJzQnZ9yMWeOtA7r69Ep6G6Ns9fl5TwdHIC6M_YSLtFGjg_z3fHq5ooqyCTgq',
      },
      file_path: {path: '/sea_pen/222.jpg'},
    },
    {
      query_info: 'a large rock sitting on top of a hill in the desert',
      url: {
        url:
            'https://lh5.googleusercontent.com/proxy/Don1aDsf2x5AOn25kN1-NdumW-Dc2QF5wbOVmn2WTpgC8ja0YfBZqqajhIXWsoqvnXdn6u57tHsAjD_ht6JywKiFFjAaum99YjAlkXuSX_Uwvi_OXuKyznUc4TR44bUlAXSYOhGeUn6pv-3vEXec',
      },
      file_path: {path: '/sea_pen/333.jpg'},
    },
  ];
  store.dispatch(seaPenAction.setRecentSeaPenImagesAction(images));
}

export async function selectRecentSeaPenImage(
    image: SeaPenWallpaper, provider: SeaPenProviderInterface,
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

  const {success} = await provider.selectRecentSeaPenImage(image.file_path);

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
