// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-smart-inputs-page' is the settings sub-page
 * to provide users with assistive or expressive input options.
 */

import '//resources/cr_elements/cr_toggle/cr_toggle.m.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_shared_css.js';

import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {PrefsBehavior} from '../prefs_behavior.js';
import {RouteObserverBehavior} from '../route_observer_behavior.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'os-settings-smart-inputs-page',

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

    /** @private */
    allowAssistivePersonalInfo_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('allowAssistivePersonalInfo');
      },
    },

    /** @private */
    allowEmojiSuggestion_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('allowEmojiSuggestion');
      },
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kShowPersonalInformationSuggestions,
        chromeos.settings.mojom.Setting.kShowEmojiSuggestions,
      ]),
    },
  },

  /**
   * @param {!Route} route
   * @param {!Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.OS_LANGUAGES_SMART_INPUTS) {
      return;
    }

    this.attemptDeepLink();
  },

  /**
   * Opens Chrome browser's autofill manage addresses setting page.
   * @private
   */
  onManagePersonalInfoClick_() {
    window.open('chrome://settings/addresses');
  },

  /**
   * @param {!Event} e
   * @private
   */
  onPersonalInfoSuggestionToggled_(e) {
    this.setPrefValue(
        'assistive_input.personal_info_enabled', e.target.checked);
  },
});
