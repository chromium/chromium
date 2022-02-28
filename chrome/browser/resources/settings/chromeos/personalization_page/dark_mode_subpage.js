// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * 'settings-dark-mode-subpage' is the setting subpage containing
 *  dark mode settings to switch between dark and light mode, theming,
 *  and a scheduler.
 */
import '../../controls/settings_radio_group.js';
import '../../controls/settings_toggle_button.js';
import '../../prefs/prefs.js';
import '//resources/cr_elements/cr_radio_button/cr_radio_button.m.js';

import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior} from '../deep_linking_behavior.js';
import {routes} from '../os_route.m.js';
import {PrefsBehavior} from '../prefs_behavior.js';
import {RouteObserverBehavior} from '../route_observer_behavior.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-dark-mode-subpage',

  behaviors: [
    RouteObserverBehavior,
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
   * RouteObserverBehavior
   * @param {!Route} route
   * @param {!Route} oldRoute
   * @protected
   */
  currentRouteChanged(route, oldRoute) {
    if (route !== routes.DARK_MODE) {
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
