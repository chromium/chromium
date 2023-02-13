// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview a singleton getter for the wallpaper mojom interface used in
 * the Personalization SWA. Also contains utility function for mocking out the
 * implementation for testing.
 */

import 'chrome://resources/mojo/mojo/public/js/bindings.js';
import 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {WallpaperProvider, WallpaperProviderInterface} from '../../personalization_app.mojom-webui.js';

let wallpaperProvider: WallpaperProviderInterface|null = null;

export function setWallpaperProviderForTesting(
    testProvider: WallpaperProviderInterface): void {
  wallpaperProvider = testProvider;
}

/** Returns a singleton for the WallpaperProvider mojom interface. */
export function getWallpaperProvider(): WallpaperProviderInterface {
  if (!wallpaperProvider) {
    wallpaperProvider = WallpaperProvider.getRemote();
  }
  return wallpaperProvider;
}
