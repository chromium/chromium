// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

const categoryLabels = {
  app_cache: loadTimeData.getString('cookieAppCache'),
  cache_storage: loadTimeData.getString('cookieCacheStorage'),
  database: loadTimeData.getString('cookieDatabaseStorage'),
  file_system: loadTimeData.getString('cookieFileSystem'),
  flash_lso: loadTimeData.getString('cookieFlashLso'),
  indexed_db: loadTimeData.getString('cookieDatabaseStorage'),
  local_storage: loadTimeData.getString('cookieLocalStorage'),
  service_worker: loadTimeData.getString('cookieServiceWorker'),
  shared_worker: loadTimeData.getString('cookieSharedWorker'),
  media_license: loadTimeData.getString('cookieMediaLicense'),
};

/**
 * 'site-data-details-subpage' Display cookie contents.
 */
Polymer({
  is: 'site-data-details-subpage',

  behaviors: [settings.RouteObserverBehavior, WebUIListenerBehavior],

  properties: {
    /**
     * The cookie entries for the given site.
     * @type {!Array<!CookieDetails>}
     * @private
     */
    entries_: Array,

    /** Set the page title on the settings-subpage parent. */
    pageTitle: {
      type: String,
      notify: true,
    },

    /** @private */
    site_: String,

    /** @private */
    siteId_: String,
  },

  /**
   * The browser proxy used to retrieve and change cookies.
   * @private {?settings.LocalDataBrowserProxy}
   */
  browserProxy_: null,

  /** @override */
  ready: function() {
    this.browserProxy_ = settings.LocalDataBrowserProxyImpl.getInstance();

    this.addWebUIListener(
        'on-tree-item-removed', this.getCookieDetails_.bind(this));
  },

  /**
   * settings.RouteObserverBehavior
   * @param {!settings.Route} route
   * @protected
   */
  currentRouteChanged: function(route) {
    if (settings.getCurrentRoute() !=
        settings.routes.SITE_SETTINGS_DATA_DETAILS) {
      return;
    }
    const site = settings.getQueryParameters().get('site');
    if (!site) {
      return;
    }
    this.site_ = site;
    this.pageTitle = loadTimeData.getStringF('siteSettingsCookieSubpage', site);
    this.getCookieDetails_();
  },

  /** @private */
  getCookieDetails_: function() {
    if (!this.site_) {
      return;
    }
    this.browserProxy_.getCookieDetails(this.site_)
        .then(
            this.onCookiesLoaded_.bind(this),
            this.onCookiesLoadFailed_.bind(this));
  },

  /**
   * @return {!Array<!CookieDataForDisplay>}
   * @private
   */
  getCookieNodes_: function(node) {
    return getCookieData(node);
  },

  /**
   * @param {!CookieList} cookies
   * @private
   */
  onCookiesLoaded_: function(cookies) {
    this.siteId_ = cookies.id;
    this.entries_ = cookies.children;
    // Set up flag for expanding cookie details.
    this.entries_.forEach(function(e) {
      e.expanded_ = false;
    });
  },

  /**
   * The site was not found. E.g. The site data may have been deleted or the
   * site URL parameter may be mistyped.
   * @private
   */
  onCookiesLoadFailed_: function() {
    this.siteId_ = '';
    this.entries_ = [];
  },

  /**
   * A handler for when the user opts to remove a single cookie.
   * @param {!CookieDetails} item
   * @return {string}
   * @private
   */
  getEntryDescription_: function(item) {
    // Frequently there are multiple cookies per site. To avoid showing a list
    // of '1 cookie', '1 cookie', ... etc, it is better to show the title of the
    // cookie to differentiate them.
    if (item.type == 'cookie') {
      return item.title;
    }
    if (item.type == 'quota') {
      return item.totalUsage;
    }
    return categoryLabels[item.type];
  },

  /**
   * A handler for when the user opts to remove a single cookie.
   * @param {!Event} event
   * @private
   */
  onRemove_: function(event) {
    this.browserProxy_.removeCookie(
        /** @type {!CookieDetails} */ (event.currentTarget.dataset).idPath);
  },

  /**
   * A handler for when the user opts to remove all cookies.
   */
  removeAll: function() {
    this.browserProxy_.removeCookie(this.siteId_);
  },
});

})();
