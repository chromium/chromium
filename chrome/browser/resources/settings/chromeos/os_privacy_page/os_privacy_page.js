// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-privacy-page' is the settings page containing privacy and
 * security settings.
 */

Polymer({
  is: 'os-settings-privacy-page',

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
        if (settings.routes.ACCOUNTS) {
          map.set(
              settings.routes.ACCOUNTS.path,
              '#manageOtherPeopleSubpageTrigger');
        }
        return map;
      },
    },

    /**
     * Whether to show the Suggested Content toggle.
     * @private
     */
    showSuggestedContentToggle_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('suggestedContentToggleEnabled');
      },
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kVerifiedAccess,
        chromeos.settings.mojom.Setting.kUsageStatsAndCrashReports,
      ]),
    },

    /**
     * True if redesign of account management flows is enabled.
     * @private
     */
    isAccountManagementFlowsV2Enabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isAccountManagementFlowsV2Enabled');
      },
      readOnly: true,
    },
  },

  /**
   * @param {!settings.Route} route
   * @param {!settings.Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== settings.routes.OS_PRIVACY) {
      return;
    }

    this.attemptDeepLink();
  },

  /** @private */
  onManageOtherPeople_() {
    settings.Router.getInstance().navigateTo(settings.routes.ACCOUNTS);
  },
});
