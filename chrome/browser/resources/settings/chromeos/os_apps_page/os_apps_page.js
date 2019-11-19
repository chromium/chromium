// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-apps-page' is the settings page containing app related settings.
 *
 */
Polymer({
  is: 'os-settings-apps-page',

  behaviors: [
    app_management.StoreClient,
    I18nBehavior,
    PrefsBehavior,
  ],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * This object holds the playStoreEnabled and settingsAppAvailable boolean.
     * @type {Object}
     */
    androidAppsInfo: Object,

    /**
     * If the Play Store app is available.
     * @type {boolean}
     */
    havePlayStoreApp: Boolean,

    /**
     * @type {string}
     */
    searchTerm: String,

    /**
     * Show ARC++ related settings and sub-page.
     * @type {boolean}
     */
    showAndroidApps: Boolean,


    /**
     * Show link to App Management.
     * @type {boolean}
     */
    showAppManagement: Boolean,

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value: function() {
        const map = new Map();
        if (settings.routes.APP_MANAGEMENT) {
          map.set(settings.routes.APP_MANAGEMENT.path, '#appManagement');
        }
        if (settings.routes.ANDROID_APPS_DETAILS) {
          map.set(
              settings.routes.ANDROID_APPS_DETAILS.path,
              '#android-apps .subpage-arrow');
        }
        return map;
      },
    },

    /**
     * @type {App}
     * @private
     */
    app_: Object,
  },

  attached: function() {
    this.watch('app_', state => app_management.util.getSelectedApp(state));
  },

  /**
   * @param {App} app
   * @return {string}
   * @private
   */
  iconUrlFromId_: function(app) {
    if (!app) {
      return '';
    }
    return app_management.util.getAppIcon(app);
  },

  /** @private */
  onClickAppManagement_: function() {
    chrome.metricsPrivate.recordEnumerationValue(
        AppManagementEntryPointsHistogramName,
        AppManagementEntryPoint.OsSettingsMainPage,
        Object.keys(AppManagementEntryPoint).length);
    settings.navigateTo(settings.routes.APP_MANAGEMENT);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onEnableAndroidAppsTap_: function(event) {
    this.setPrefValue('arc.enabled', true);
    event.stopPropagation();
  },

  /**
   * @return {boolean}
   * @private
   */
  isEnforced_: function(pref) {
    return pref.enforcement == chrome.settingsPrivate.Enforcement.ENFORCED;
  },

  /** @private */
  onAndroidAppsSubpageTap_: function(event) {
    if (event.target && event.target.tagName == 'A') {
      // Filter out events coming from 'Learn more' link
      return;
    }
    if (this.androidAppsInfo.playStoreEnabled) {
      settings.navigateTo(settings.routes.ANDROID_APPS_DETAILS);
    }
  },

  /**
   * @param {!MouseEvent} event
   * @private
   */
  onManageAndroidAppsTap_: function(event) {
    // |event.detail| is the click count. Keyboard events will have 0 clicks.
    const isKeyboardAction = event.detail == 0;
    settings.AndroidAppsBrowserProxyImpl.getInstance().showAndroidAppsSettings(
        isKeyboardAction);
  },
});
