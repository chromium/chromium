// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-files-page' is the settings page containing files settings.
 *
 */
Polymer({
  is: 'os-settings-files-page',

  behaviors: [
    DeepLinkingBehavior,
    settings.RouteObserverBehavior,
  ],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value() {
        const map = new Map();
        if (settings.routes.SMB_SHARES) {
          map.set(settings.routes.SMB_SHARES.path, '#smbShares');
        }
        return map;
      },
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () =>
          new Set([chromeos.settings.mojom.Setting.kGoogleDriveConnection]),
    },
  },

  /**
   * @param {!settings.Route} route
   * @param {!settings.Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== settings.routes.FILES) {
      return;
    }

    this.attemptDeepLink();
  },

  /** @private */
  onTapSmbShares_() {
    settings.Router.getInstance().navigateTo(settings.routes.SMB_SHARES);
  },
});
