// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-details' show the details (permissions and usage) for a given origin
 * under Site Settings.
 */
Polymer({
  is: 'site-details',

  behaviors: [
    I18nBehavior, SiteSettingsBehavior, settings.RouteObserverBehavior,
    WebUIListenerBehavior
  ],

  properties: {
    /**
     * Whether unified autoplay blocking is enabled.
     */
    blockAutoplayEnabled: Boolean,

    /**
     * Use the string representing the origin or extension name as the page
     * title of the settings-subpage parent.
     */
    pageTitle: {
      type: String,
      notify: true,
    },

    /**
     * The origin that this widget is showing details for.
     * @private
     */
    origin_: String,

    /**
     * The amount of data stored for the origin.
     * @private
     */
    storedData_: {
      type: String,
      value: '',
    },

    /**
     * The number of cookies stored for the origin.
     * @private
     */
    numCookies_: {
      type: String,
      value: '',
    },

    /** @private */
    enableExperimentalWebPlatformFeatures_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('enableExperimentalWebPlatformFeatures');
      },
    },

    /** @private */
    enableNativeFileSystemWriteContentSetting_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean(
            'enableNativeFileSystemWriteContentSetting');
      }
    },

    /** @private */
    enableInsecureContentContentSetting_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('enableInsecureContentContentSetting');
      }
    },
  },

  listeners: {
    'usage-deleted': 'onUsageDeleted_',
  },

  /** @override */
  attached: function() {
    this.addWebUIListener(
        'contentSettingSitePermissionChanged',
        this.onPermissionChanged_.bind(this));

    // <if expr="chromeos">
    this.addWebUIListener(
        'prefEnableDrmChanged', this.prefEnableDrmChanged_.bind(this));
    // </if>

    // Refresh block autoplay status from the backend.
    this.browserProxy.fetchBlockAutoplayStatus();
  },

  /** @override */
  ready: function() {
    this.ContentSettingsTypes = settings.ContentSettingsTypes;
  },

  /**
   * settings.RouteObserverBehavior
   * @param {!settings.Route} route
   * @protected
   */
  currentRouteChanged: function(route) {
    if (route != settings.routes.SITE_SETTINGS_SITE_DETAILS) {
      return;
    }
    const site = settings.getQueryParameters().get('site');
    if (!site) {
      return;
    }
    this.origin_ = site;
    this.browserProxy.isOriginValid(this.origin_).then((valid) => {
      if (!valid) {
        settings.navigateToPreviousRoute();
      } else {
        this.$.usageApi.fetchUsageTotal(this.toUrl(this.origin_).hostname);
        this.updatePermissions_(this.getCategoryList());
      }
    });
  },

  /**
   * Called when a site within a category has been changed.
   * @param {!settings.ContentSettingsTypes} category The category that
   *     changed.
   * @param {string} origin The origin of the site that changed.
   * @param {string} embeddingOrigin The embedding origin of the site that
   *     changed.
   * @private
   */
  onPermissionChanged_: function(category, origin, embeddingOrigin) {
    if (this.origin_ === undefined || this.origin_ == '' ||
        origin === undefined || origin == '') {
      return;
    }
    if (!this.getCategoryList().includes(category)) {
      return;
    }

    // Site details currently doesn't support embedded origins, so ignore it
    // and just check whether the origins are the same.
    this.updatePermissions_([category]);
  },

  // <if expr="chromeos">
  prefEnableDrmChanged_: function() {
    this.updatePermissions_([settings.ContentSettingsTypes.PROTECTED_CONTENT]);
  },
  // </if>

  /**
   * Retrieves the permissions listed in |categoryList| from the backend for
   * |this.origin_|.
   * @param {!Array<!settings.ContentSettingsTypes>} categoryList The list
   *     of categories to update permissions for.
   * @private
   */
  updatePermissions_: function(categoryList) {
    const permissionsMap =
        /**
         * @type {!Object<!settings.ContentSettingsTypes,
         *         !SiteDetailsPermissionElement>}
         */
        (Array.prototype.reduce.call(
            this.root.querySelectorAll('site-details-permission'),
            (map, element) => {
              if (categoryList.includes(element.category)) {
                map[element.category] = element;
              }
              return map;
            },
            {}));

    this.browserProxy.getOriginPermissions(this.origin_, categoryList)
        .then((exceptionList) => {
          exceptionList.forEach((exception, i) => {
            // |exceptionList| should be in the same order as
            // |categoryList|.
            if (permissionsMap[categoryList[i]]) {
              permissionsMap[categoryList[i]].site = exception;
            }
          });

          // The displayName won't change, so just use the first
          // exception.
          assert(exceptionList.length > 0);
          this.pageTitle = exceptionList[0].displayName;
        });
  },

  /** @private */
  onCloseDialog_: function(e) {
    e.target.closest('cr-dialog').close();
  },

  /**
   * Confirms the resetting of all content settings for an origin.
   * @param {!Event} e
   * @private
   */
  onConfirmClearSettings_: function(e) {
    e.preventDefault();
    this.$.confirmResetSettings.showModal();
  },

  /**
   * Confirms the clearing of storage for an origin.
   * @param {!Event} e
   * @private
   */
  onConfirmClearStorage_: function(e) {
    e.preventDefault();
    this.$.confirmClearStorage.showModal();
  },

  /**
   * Resets all permissions for the current origin.
   * @private
   */
  onResetSettings_: function(e) {
    this.browserProxy.setOriginPermissions(
        this.origin_, this.getCategoryList(), settings.ContentSetting.DEFAULT);
    if (this.getCategoryList().includes(
            settings.ContentSettingsTypes.PLUGINS)) {
      this.browserProxy.clearFlashPref(this.origin_);
    }

    this.onCloseDialog_(e);
  },

  /**
   * Clears all data stored, except cookies, for the current origin.
   * @private
   */
  onClearStorage_: function(e) {
    if (this.hasUsage_(this.storedData_, this.numCookies_)) {
      this.$.usageApi.clearUsage(this.toUrl(this.origin_).href);
    }

    this.onCloseDialog_(e);
  },

  /**
   * Called when usage has been deleted for an origin via a non-Site Details
   * source, e.g. clear browsing data.
   * @param {!CustomEvent<!{origin: string}>} event
   * @private
   */
  onUsageDeleted_: function(event) {
    if (event.detail.origin == this.toUrl(this.origin_).href) {
      this.storedData_ = '';
      this.numCookies_ = '';
    }
  },

  /**
   * Checks whether this site has any usage information to show.
   * @return {boolean} Whether there is any usage information to show (e.g.
   *     disk or battery).
   * @private
   */
  hasUsage_: function(storage, cookies) {
    return storage != '' || cookies != '';
  },

  /**
   * Checks whether this site has both storage and cookies information to show.
   * @return {boolean} Whether there are both storage and cookies information to
   *     show.
   * @private
   */
  hasDataAndCookies_: function(storage, cookies) {
    return storage != '' && cookies != '';
  },

  /** @private */
  onResetSettingsDialogClosed_: function() {
    cr.ui.focusWithoutInk(assert(this.$$('#resetSettingsButton')));
  },

  /** @private */
  onClearStorageDialogClosed_: function() {
    cr.ui.focusWithoutInk(assert(this.$$('#clearStorage')));
  },
});
