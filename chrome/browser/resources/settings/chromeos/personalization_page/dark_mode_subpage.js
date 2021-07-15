// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * 'settings-dark-mode-subpage' is the setting subpage containing
 *  dark mode settings to switch between dark and light mode, theming,
 *  and a scheduler.
 */
Polymer({
  is: 'settings-dark-mode-subpage',

  behaviors: [
    settings.RouteObserverBehavior,
    DeepLinkingBehavior,
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
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kDarkModeOnOff,
        chromeos.settings.mojom.Setting.kDarkModeThemed
      ]),
    },

  },

  /**
   * settings.RouteObserverBehavior
   * @param {!settings.Route} route
   * @param {!settings.Route} oldRoute
   * @protected
   */
  currentRouteChanged(route, oldRoute) {
    if (route !== settings.routes.DARK_MODE) {
      return;
    }
    this.attemptDeepLink();
  },

  /**
   * @return {string}
   * @private
   */
  getDarkModeOnOffLabel_() {
    return this.i18n(
        this.getPref('ash.dark_mode.enabled').value ? 'darkModeOn' :
                                                      'darkModeOff');
  },
});