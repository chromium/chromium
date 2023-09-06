// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {isGooglePhotosIntegrationEnabled, isPersonalizationJellyEnabled, isTimeOfDayWallpaperEnabled} from './load_time_booleans.js';
import {Paths, PersonalizationRouterElement} from './personalization_router_element.js';
import {PersonalizationStore} from './personalization_store.js';
import {getThemeProvider} from './theme/theme_interface_provider.js';
import {DEFAULT_COLOR_SCHEME} from './theme/utils.js';
import {isNonEmptyArray} from './utils.js';
import {setFullscreenEnabledAction} from './wallpaper/wallpaper_actions.js';
import {selectGooglePhotosAlbum, selectWallpaper, setDailyRefreshCollectionId} from './wallpaper/wallpaper_controller.js';
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

  if (isPersonalizationJellyEnabled()) {
    // Turn on dynamic color with default scheme.
    const themeProvider = getThemeProvider();
    themeProvider.setColorScheme(DEFAULT_COLOR_SCHEME);
    const {colorScheme} = await themeProvider.getColorScheme();
    assert(
        colorScheme === DEFAULT_COLOR_SCHEME, 'reset to default color scheme');
  }

  const router = PersonalizationRouterElement.instance();
  router.goToRoute(Paths.ROOT);
}

async function selectTimeOfDayWallpaper() {
  assert(isTimeOfDayWallpaperEnabled(), 'time of day must be enabled');
  const store = PersonalizationStore.getInstance();
  assert(!!store);
  const id = loadTimeData.getString('timeOfDayWallpaperCollectionId');
  const images = store.data.wallpaper.backdrop.images[id];
  assert(isNonEmptyArray(images), 'time of day collection images must exist');
  const image = images[0];
  await selectWallpaper(image, getWallpaperProvider(), store);
}

async function enableDailyRefresh(collectionId: string) {
  const store = PersonalizationStore.getInstance();
  assert(!!store);
  await setDailyRefreshCollectionId(
      collectionId, getWallpaperProvider(), store);
}

async function disableDailyRefresh() {
  const store = PersonalizationStore.getInstance();
  assert(!!store);
  await setDailyRefreshCollectionId('', getWallpaperProvider(), store);
}

async function enableDailyGooglePhotosRefresh(albumId: string) {
  const store = PersonalizationStore.getInstance();
  assert(!!store);
  await selectGooglePhotosAlbum(albumId, getWallpaperProvider(), store);
}

declare global {
  interface Window {
    personalizationTestApi: {
      enterFullscreen: () => void,
      isGooglePhotosIntegrationEnabled: () => boolean,
      makeTransparent: () => void,
      reset: () => Promise<void>,
      selectTimeOfDayWallpaper: () => Promise<void>,
      enableDailyRefresh: (collectionId: string) => Promise<void>,
      disableDailyRefresh: () => Promise<void>,
      enableDailyGooglePhotosRefresh: (albumId: string) => Promise<void>,
    };
  }
}

window.personalizationTestApi = {
  enterFullscreen,
  isGooglePhotosIntegrationEnabled,
  makeTransparent,
  reset,
  selectTimeOfDayWallpaper,
  enableDailyRefresh,
  disableDailyRefresh,
  enableDailyGooglePhotosRefresh,
};
