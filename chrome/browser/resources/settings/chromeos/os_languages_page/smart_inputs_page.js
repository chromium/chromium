// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-smart-inputs-page' is the settings sub-page
 * to provide users with assistive or expressive input options.
 */

Polymer({
  is: 'os-settings-smart-inputs-page',

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
   * @param {!settings.Route} route
   * @param {!settings.Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== settings.routes.OS_LANGUAGES_SMART_INPUTS) {
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
