// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview a singleton getter for the mojom interface used in
 * the Personalization SWA. Also contains utility functions around fetching
 * mojom data and mocking out the implementation for testing.
 */

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';
// file_path is not available at chrome://resources and is copied here for use.
import './file_path.mojom-lite.js';
import './personalization_app.mojom-lite.js';

/** @type {?ash.personalizationApp.mojom.WallpaperProviderInterface} */
let wallpaperProvider = null;

/**
 * @param {!ash.personalizationApp.mojom.WallpaperProviderInterface}
 *     testProvider
 */
export function setWallpaperProviderForTesting(testProvider) {
  wallpaperProvider = testProvider;
}

/**
 * Returns a singleton for the WallpaperProvider mojom interface.
 * @return {!ash.personalizationApp.mojom.WallpaperProviderInterface}
 */
export function getWallpaperProvider() {
  if (!wallpaperProvider) {
    wallpaperProvider =
        ash.personalizationApp.mojom.WallpaperProvider.getRemote();
    wallpaperProvider.makeTransparent();
  }
  return wallpaperProvider;
}
