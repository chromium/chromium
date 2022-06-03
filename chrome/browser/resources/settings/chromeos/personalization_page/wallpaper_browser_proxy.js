// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';
// clang-format on

/** @interface */
export class WallpaperBrowserProxy {
  /**
   * @return {!Promise<boolean>} Whether the wallpaper setting row should be
   *     visible.
   */
  isWallpaperSettingVisible() {}

  /**
   * @return {!Promise<boolean>} Whether the wallpaper is policy controlled.
   */
  isWallpaperPolicyControlled() {}

  openWallpaperManager() {}
}

/**
 * @implements {WallpaperBrowserProxy}
 */
export class WallpaperBrowserProxyImpl {
  /** @override */
  isWallpaperSettingVisible() {
    return sendWithPromise('isWallpaperSettingVisible');
  }

  /** @override */
  isWallpaperPolicyControlled() {
    return sendWithPromise('isWallpaperPolicyControlled');
  }

  /** @override */
  openWallpaperManager() {
    chrome.send('openWallpaperManager');
  }
}

addSingletonGetter(WallpaperBrowserProxyImpl);
