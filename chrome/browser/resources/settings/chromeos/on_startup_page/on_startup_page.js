// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-on-startup-page' is the settings page containing the on_startup
 * setting to allow the restoration in Chrome OS.
 */

Polymer({
  is: 'settings-on-startup-page',

  behaviors: [
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
     * Enum values for the 'settings.on_startup' preference.
     * @private {!Object<string, number>}
     */
    prefValues_: {
      readOnly: true,
      type: Object,
      value: {
        ALWAYS: 1,
        ASK_EVERY_TIME: 2,
        DO_NOT_RESTORE: 3,
      },
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kRestoreAppsAndPages,
      ]),
    },
  },

  /**
   * @param {!settings.Route} route
   * @param {!settings.Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== settings.routes.ON_STARTUP) {
      return;
    }

    this.attemptDeepLink();
  },

  /**
   * Used to convert the restore apps and pages preference number to a string
   * for radio buttons.
   * @param {number} value
   * @return {string}
   * @private
   */
  getName_(value) {
    return value.toString();
  },
});
