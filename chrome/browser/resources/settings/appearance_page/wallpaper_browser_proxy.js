// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {
  /** @interface */
  class WallpaperBrowserProxy {
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
  class WallpaperBrowserProxyImpl {
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

  return {
    WallpaperBrowserProxy: WallpaperBrowserProxy,
    WallpaperBrowserProxyImpl: WallpaperBrowserProxyImpl,
  };
});
