// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.m.js';

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

/** @type {?WallpaperBrowserProxy} */
let instance = null;

/**
 * @implements {WallpaperBrowserProxy}
 */
export class WallpaperBrowserProxyImpl {
  /** @return {!WallpaperBrowserProxy} */
  static getInstance() {
    return instance || (instance = new WallpaperBrowserProxyImpl());
  }

  /** @param {!WallpaperBrowserProxy} obj */
  static setInstance(obj) {
    instance = obj;
  }

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
