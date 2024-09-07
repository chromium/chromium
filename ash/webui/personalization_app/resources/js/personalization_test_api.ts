// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isNonEmptyArray} from 'chrome://resources/ash/common/sea_pen/sea_pen_utils.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {isGooglePhotosIntegrationEnabled, isTimeOfDayWallpaperEnabled} from './load_time_booleans.js';
import {Paths, PersonalizationRouterElement} from './personalization_router_element.js';
import {PersonalizationStore} from './personalization_store.js';
import {getThemeProvider} from './theme/theme_interface_provider.js';
import {DEFAULT_COLOR_SCHEME} from './theme/utils.js';
import {selectGooglePhotosAlbum, selectWallpaper, setDailyRefreshCollectionId} from './wallpaper/wallpaper_controller.js';
import {setShouldWaitForFullscreenOpacityTransitionsForTesting} from './wallpaper/wallpaper_fullscreen_element.js';
import {getWallpaperProvider} from './wallpaper/wallpaper_interface_provider.js';

/**
 * @fileoverview provides useful functions for e2e browsertests.
 */

function makeTransparent() {
  setShouldWaitForFullscreenOpacityTransitionsForTesting(false);
  const wallpaperProvider = getWallpaperProvider();
  wallpaperProvider.makeTransparent();
}

// Reset to a default state at the root of the app. Useful for browsertests.
async function reset() {
  await selectDefaultWallpaperImage();
  goToRootPath();
}

function goToRootPath() {
  const router = PersonalizationRouterElement.instance();
  router.goToRoute(Paths.ROOT);
}

async function selectDefaultWallpaperImage() {
  const wallpaperProvider = getWallpaperProvider();
  await wallpaperProvider.selectDefaultImage();
}

async function setDefaultColorScheme() {
  // Turn on dynamic color with default scheme.
  const themeProvider = getThemeProvider();
  themeProvider.setColorScheme(DEFAULT_COLOR_SCHEME);
  const {colorScheme} = await themeProvider.getColorScheme();
  assert(colorScheme === DEFAULT_COLOR_SCHEME, 'reset to default color scheme');
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
      disableDailyRefresh: () => Promise<void>,
      enableDailyGooglePhotosRefresh: (albumId: string) => Promise<void>,
      enableDailyRefresh: (collectionId: string) => Promise<void>,
      goToRootPath: () => void,
      isGooglePhotosIntegrationEnabled: () => boolean,
      makeTransparent: () => void,
      reset: () => Promise<void>,
      selectDefaultWallpaperImage: () => Promise<void>,
      selectTimeOfDayWallpaper: () => Promise<void>,
      setDefaultColorScheme: () => Promise<void>,
    };
  }
}

window.personalizationTestApi = {
  disableDailyRefresh,
  enableDailyGooglePhotosRefresh,
  enableDailyRefresh,
  goToRootPath,
  isGooglePhotosIntegrationEnabled,
  makeTransparent,
  reset,
  selectDefaultWallpaperImage,
  selectTimeOfDayWallpaper,
  setDefaultColorScheme,
};
