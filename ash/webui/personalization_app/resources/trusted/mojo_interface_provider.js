// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview a singleton getter for the mojom interface used in
 * the Personalization SWA. Also contains utility functions around fetching
 * mojom data and mocking out the implementation for testing.
 */

import 'chrome://resources/mojo/mojo/public/js/bindings.js';
import 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {WallpaperProvider} from './personalization_app.mojom-webui.js';

/** @type {?WallpaperProviderInterface} */
let wallpaperProvider = null;

/**
 * @param {!WallpaperProviderInterface}
 *     testProvider
 */
export function setWallpaperProviderForTesting(testProvider) {
  wallpaperProvider = testProvider;
}

/**
 * Returns a singleton for the WallpaperProvider mojom interface.
 * @return {!WallpaperProviderInterface}
 */
export function getWallpaperProvider() {
  if (!wallpaperProvider) {
    wallpaperProvider = WallpaperProvider.getRemote();
    wallpaperProvider.makeTransparent();
  }
  return wallpaperProvider;
}
