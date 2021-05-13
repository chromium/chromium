// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-search-subpage' is the settings sub-page containing
 * search engine and quick answers settings.
 */
Polymer({
  is: 'settings-search-subpage',

  behaviors: [
    DeepLinkingBehavior,
    I18nBehavior,
    settings.RouteObserverBehavior,
  ],

  properties: {
    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kPreferredSearchEngine,
        chromeos.settings.mojom.Setting.kQuickAnswersOnOff,
      ]),
    },
  },

  /**
   * @param {!settings.Route} route
   * @param {!settings.Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== settings.routes.SEARCH_SUBPAGE) {
      return;
    }

    this.attemptDeepLink();
  },
});
