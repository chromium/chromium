// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';

import {Events, EventType, untrustedOrigin} from '../../common/constants.js';
import {validateReceivedSelection} from '../iframe_api.js';
import {PersonalizationRouter} from '../personalization_router_element.js';
import {PersonalizationStore} from '../personalization_store.js';

import {selectWallpaper} from './wallpaper_controller.js';
import {getWallpaperProvider} from './wallpaper_interface_provider.js';

/**
 * @fileoverview message handler that receives data from untrusted.
 */

export function onMessageReceived(event: MessageEvent) {
  assert(
      event.origin === untrustedOrigin, 'Message not from the correct origin');

  const store = PersonalizationStore.getInstance();

  const data = event.data as Events;
  switch (data.type) {
    case EventType.SELECT_COLLECTION:
      const collections = store.data.wallpaper.backdrop.collections;

      const selectedCollection = validateReceivedSelection(event, collections);
      PersonalizationRouter.instance().selectCollection(selectedCollection);
      break;
    case EventType.SELECT_GOOGLE_PHOTOS_COLLECTION:
      PersonalizationRouter.instance().selectGooglePhotosCollection();
      break;
    case EventType.SELECT_LOCAL_COLLECTION:
      PersonalizationRouter.instance().selectLocalCollection();
      break;
    case EventType.SELECT_IMAGE:
      const collectionId = PersonalizationRouter.instance().collectionId;
      if (!collectionId) {
        console.warn('collectionId is not available when selecting image.');
        return;
      }
      const images = store.data.wallpaper.backdrop.images[collectionId];
      const selectedImage = validateReceivedSelection(event, images);
      selectWallpaper(selectedImage, getWallpaperProvider(), store);
      break;
  }
}
