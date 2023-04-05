// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';

import {isGooglePhotosIntegrationEnabled} from './load_time_booleans.js';
import {Paths, PersonalizationRouter} from './personalization_router_element.js';
import {PersonalizationStore} from './personalization_store.js';
import {setFullscreenEnabledAction} from './wallpaper/wallpaper_actions.js';
import {getWallpaperProvider} from './wallpaper/wallpaper_interface_provider.js';

/**
 * @fileoverview provides useful functions for e2e browsertests.
 */

function enterFullscreen() {
  const store = PersonalizationStore.getInstance();
  assert(!!store);
  store.dispatch(setFullscreenEnabledAction(true));
}

function makeTransparent() {
  const wallpaperProvider = getWallpaperProvider();
  wallpaperProvider.makeTransparent();
}

// Reset to a default state at the root of the app. Useful for browsertests.
async function reset() {
  const wallpaperProvider = getWallpaperProvider();
  await wallpaperProvider.selectDefaultImage();
  const router = PersonalizationRouter.instance();
  router.goToRoute(Paths.ROOT);
}

declare global {
  interface Window {
    personalizationTestApi: {
      enterFullscreen: () => void,
      isGooglePhotosIntegrationEnabled: () => boolean,
      makeTransparent: () => void,
      reset: () => Promise<void>,
    };
  }
}

window.personalizationTestApi = {
  enterFullscreen,
  isGooglePhotosIntegrationEnabled,
  makeTransparent,
  reset,
};
