// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '/assert.m.js';
import {EventType, untrustedOrigin} from '../common/constants.js';
import {validateReceivedSelection} from '../common/iframe_api.js';
import {getWallpaperProvider} from './mojo_interface_provider.js';
import {selectWallpaper} from './personalization_controller.js';
import {PersonalizationRouter} from './personalization_router_element.js';
import {PersonalizationStore} from './personalization_store.js';

/**
 * @fileoverview message handler that receives data from untrusted.
 */

/**
 * @param {!Event} event
 */
export function onMessageReceived(event) {
  assert(
      event.origin === untrustedOrigin, 'Message not from the correct origin');

  const store = PersonalizationStore.getInstance();

  switch (event.data.type) {
    case EventType.SELECT_COLLECTION:
      const collections = store.data.backdrop.collections;

      const selectedCollection = validateReceivedSelection(event, collections);
      PersonalizationRouter.instance().selectCollection(selectedCollection);
      break;
    case EventType.SELECT_LOCAL_COLLECTION:
      PersonalizationRouter.instance().selectLocalCollection();
      break;
    case EventType.SELECT_IMAGE:
      const collectionId = PersonalizationRouter.instance().collectionId;
      const images = store.data.backdrop.images[collectionId];
      const selectedImage = validateReceivedSelection(event, images);
      selectWallpaper(selectedImage, getWallpaperProvider(), store);
      break;
  }
}
