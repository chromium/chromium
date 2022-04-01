// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';

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

declare global {
  interface Window {
    personalizationTestApi: {
      enterFullscreen: () => void,
      makeTransparent: () => void,
    };
  }
}

window.personalizationTestApi = {
  enterFullscreen,
  makeTransparent
};
