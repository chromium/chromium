// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-wallpaper-page' is the settings sub-page containing
 * wallpaper settings.
 */
Polymer({
  is: 'settings-wallpaper-page',

  behaviors: [],

  properties: {},

  /** @private {?settings.WallpaperBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = settings.WallpaperBrowserProxyImpl.getInstance();
  },

  ready() {},
});
