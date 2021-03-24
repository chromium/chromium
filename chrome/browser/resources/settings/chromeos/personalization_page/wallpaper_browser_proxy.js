// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';
// clang-format on

cr.define('settings', function() {
  /** @interface */
  /* #export */ class WallpaperBrowserProxy {
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
   * @implements {settings.WallpaperBrowserProxy}
   */
  /* #export */ class WallpaperBrowserProxyImpl {
    /** @override */
    isWallpaperSettingVisible() {
      return cr.sendWithPromise('isWallpaperSettingVisible');
    }

    /** @override */
    isWallpaperPolicyControlled() {
      return cr.sendWithPromise('isWallpaperPolicyControlled');
    }

    /** @override */
    openWallpaperManager() {
      chrome.send('openWallpaperManager');
    }
  }

  cr.addSingletonGetter(WallpaperBrowserProxyImpl);

  // #cr_define_end
  return {
    WallpaperBrowserProxy: WallpaperBrowserProxy,
    WallpaperBrowserProxyImpl: WallpaperBrowserProxyImpl,
  };
});
