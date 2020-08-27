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
    DeepLinkingBehavior,
    I18nBehavior,
    PrefsBehavior,
    settings.RouteObserverBehavior,
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
     * Show Plugin VM shared folders sub-page.
     * @type {boolean}
     */
    showPluginVm: Boolean,

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value() {
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

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kManageAndroidPreferences,
        chromeos.settings.mojom.Setting.kTurnOnPlayStore,
      ]),
    },
  },

  attached() {
    this.watch('app_', state => app_management.util.getSelectedApp(state));
  },

  /**
   * @param {!settings.Route} route
   * @param {!settings.Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== settings.routes.APPS) {
      return;
    }

    this.attemptDeepLink();
  },

  /**
   * @param {App} app
   * @return {string}
   * @private
   */
  iconUrlFromId_(app) {
    if (!app) {
      return '';
    }
    return app_management.util.getAppIcon(app);
  },

  /** @private */
  onClickAppManagement_() {
    chrome.metricsPrivate.recordEnumerationValue(
        AppManagementEntryPointsHistogramName,
        AppManagementEntryPoint.OsSettingsMainPage,
        Object.keys(AppManagementEntryPoint).length);
    settings.Router.getInstance().navigateTo(settings.routes.APP_MANAGEMENT);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onEnableAndroidAppsTap_(event) {
    this.setPrefValue('arc.enabled', true);
    event.stopPropagation();
  },

  /**
   * @return {boolean}
   * @private
   */
  isEnforced_(pref) {
    return pref.enforcement == chrome.settingsPrivate.Enforcement.ENFORCED;
  },

  /** @private */
  onAndroidAppsSubpageTap_(event) {
    if (this.androidAppsInfo.playStoreEnabled) {
      settings.Router.getInstance().navigateTo(
          settings.routes.ANDROID_APPS_DETAILS);
    }
  },

  /**
   * @param {!MouseEvent} event
   * @private
   */
  onManageAndroidAppsTap_(event) {
    // |event.detail| is the click count. Keyboard events will have 0 clicks.
    const isKeyboardAction = event.detail == 0;
    settings.AndroidAppsBrowserProxyImpl.getInstance().showAndroidAppsSettings(
        isKeyboardAction);
  },
});
