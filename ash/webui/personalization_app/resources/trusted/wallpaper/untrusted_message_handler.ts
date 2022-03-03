// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Events, EventType} from '../../common/constants.js';
import {IFrameApi} from '../iframe_api.js';
import {Paths, PersonalizationRouter} from '../personalization_router_element.js';
import {PersonalizationStore} from '../personalization_store.js';

import {selectWallpaper} from './wallpaper_controller.js';
import {getWallpaperProvider} from './wallpaper_interface_provider.js';

/**
 * @fileoverview message handler that receives data from untrusted.
 */

export function onMessageReceived(data: Events) {
  const store = PersonalizationStore.getInstance();

  switch (data.type) {
    case EventType.SELECT_COLLECTION:
      const collections = store.data.wallpaper.backdrop.collections;

      const selectedCollection =
          IFrameApi.getInstance().validateReceivedSelection(data, collections);
      PersonalizationRouter.instance().selectCollection(selectedCollection);
      break;
    case EventType.SELECT_GOOGLE_PHOTOS_COLLECTION:
      PersonalizationRouter.instance().goToRoute(Paths.GooglePhotosCollection);
      break;
    case EventType.SELECT_LOCAL_COLLECTION:
      PersonalizationRouter.instance().goToRoute(Paths.LocalCollection);
      break;
    case EventType.SELECT_IMAGE:
      const collectionId = PersonalizationRouter.instance().collectionId;
      if (!collectionId) {
        console.warn('collectionId is not available when selecting image.');
        return;
      }
      const images = store.data.wallpaper.backdrop.images[collectionId];
      const selectedImage =
          IFrameApi.getInstance().validateReceivedSelection(data, images);
      selectWallpaper(selectedImage, getWallpaperProvider(), store);
      break;
  }
}
