// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Events, EventType} from '../../common/constants.js';
import {IFrameApi} from '../iframe_api.js';
import {Paths, PersonalizationRouter} from '../personalization_router_element.js';
import {PersonalizationStore} from '../personalization_store.js';

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
      PersonalizationRouter.instance().goToRoute(
          Paths.GOOGLE_PHOTOS_COLLECTION);
      break;
    case EventType.SELECT_LOCAL_COLLECTION:
      PersonalizationRouter.instance().goToRoute(Paths.LOCAL_COLLECTION);
      break;
  }
}
