// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-wallpaper-images-page' is the settings sub-page
 * to display wallpaper images for a given wallpaper collection. A query
 * parameter for the wallpaper collection id is required.
 */
Polymer({
  is: 'settings-wallpaper-images-page',

  behaviors: [
    settings.RouteObserverBehavior,
  ],

  properties: {
    /**
     * @private
     * @type {?Array<!WallpaperImage>}
     */
    images_: {
      type: Array,
      value: null,
    },

    /** @private */
    loading_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    error_: {
      type: Boolean,
      computed: 'computeError_(images_, loading_)',
    },

    /** @private */
    success_: {
      type: Boolean,
      computed: 'computeSuccess_(images_, loading_)',
    },

    /**
     * Stores a mapping of collection id to array of wallpaper images. Used to
     * skip making another network request if user navigates to a wallpaper
     * collection that has already been viewed.
     * @private
     * @type {!Map<!string, !Array<!WallpaperImage>>}
     */
    cache_: {
      type: Map,
      value: new Map(),
    }
  },

  /** @private {?settings.WallpaperBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = settings.WallpaperBrowserProxyImpl.getInstance();
  },

  /** @override */
  detached() {
    this.cache_.clear();
  },

  /**
   * RouteObserverBehavior
   * @param {?settings.Route} newRoute
   * @param {?settings.Route} oldRoute
   */
  currentRouteChanged(newRoute, oldRoute) {
    if (newRoute !== settings.routes.WALLPAPER_IMAGES) {
      return;
    }
    this.fetchImages_();
  },

  /** @private */
  async fetchImages_() {
    this.images_ = null;

    const queryParameters = settings.Router.getInstance().getQueryParameters();
    const collectionId = queryParameters.get('collection');
    assert(
        (typeof collectionId === 'string') && collectionId.length > 0,
        'Collection id query parameter is required');

    if (this.cache_.has(collectionId)) {
      this.images_ = this.cache_.get(collectionId);
      return;
    }

    this.loading_ = true;
    try {
      this.images_ = /** @type {!Array<!WallpaperImage>} */ (
          await this.browserProxy_.fetchImagesForCollection(collectionId));
      this.cache_.set(collectionId, this.images_);
    } catch (e) {
      // TODO(b/181697575) handle errors and allow user to retry
      console.warn(
          'Fetching wallpaper collection images failed for collection id',
          collectionId);
    } finally {
      this.loading_ = false;
    }
  },

  /**
   * @private
   * @param {?Array<!WallpaperImage>} images
   * @param {boolean} loading
   * @return {boolean}
   */
  computeError_(images, loading) {
    return !loading && !this.computeSuccess_(images, loading);
  },

  /**
   * @private
   * @param {?Array<!WallpaperImage>} images
   * @param {boolean} loading
   * @return {boolean}
   */
  computeSuccess_(images, loading) {
    return !loading && Array.isArray(images) && images.length > 0;
  },
});
