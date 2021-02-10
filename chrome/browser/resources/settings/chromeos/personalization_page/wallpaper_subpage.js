// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-wallpaper-page' is the settings sub-page containing
 * wallpaper settings.
 */
Polymer({
  is: 'settings-wallpaper-page',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /**
     * @private
     * @type {?Array<!WallpaperCollection>}
     */
    collections_: {
      type: Array,
      value: null,
    },

    /** @private */
    loading_: {
      type: Boolean,
      value: false,
    },

    /**
     * @private
     * @type {boolean}
     */
    error_: {
      type: Boolean,
      computed: 'computeError_(collections_, loading_)',
    },

    /**
     * @private
     * @type {boolean}
     */
    success_: {
      type: Boolean,
      computed: 'computeSuccess_(collections_, loading_)',
    },
  },

  /** @private {?settings.WallpaperBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = settings.WallpaperBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready() {
    this.fetchWallpaperCollections_();
  },

  /** @private */
  async fetchWallpaperCollections_() {
    this.loading_ = true;
    this.collections_ = null;
    try {
      this.collections_ = await this.browserProxy_.fetchWallpaperCollections();
    } catch (e) {
      console.warn('Fetching wallpaper collections failed');
    } finally {
      this.loading_ = false;
    }
  },

  /**
   * @private
   * @param {?Array<!WallpaperCollection>} collections
   * @param {boolean} loading
   * @return {boolean}
   */
  computeError_(collections, loading) {
    return !loading && !this.computeSuccess_(collections, loading);
  },

  /**
   * @private
   * @param {?Array<!WallpaperCollection>} collections
   * @param {boolean} loading
   * @return {boolean}
   */
  computeSuccess_(collections, loading) {
    return !loading && Array.isArray(collections) && collections.length > 0;
  },
});
