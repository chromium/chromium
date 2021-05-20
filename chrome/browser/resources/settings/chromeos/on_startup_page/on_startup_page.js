// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-on-startup-page' is the settings page containing the on_startup
 * setting to allow the restoration in Chrome OS.
 */

import '../../prefs/prefs.js';
import '../../settings_page/settings_animated_pages.js';
import '../../settings_page/settings_subpage.js';
import '../../settings_shared_css.js';

import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {PrefsBehavior} from '../../prefs/prefs_behavior.js';
import {Route, RouteObserverBehavior, Router} from '../../router.js';
import {DeepLinkingBehavior} from '../deep_linking_behavior.m.js';
import {routes} from '../os_route.m.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-on-startup-page',

  behaviors: [
    DeepLinkingBehavior,
    I18nBehavior,
    PrefsBehavior,
    RouteObserverBehavior,
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
   * @param {!Route} route
   * @param {!Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.ON_STARTUP) {
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
