// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-search-page' contains search and assistant settings.
 */
Polymer({
  is: 'os-settings-search-page',

  behaviors: [
    DeepLinkingBehavior,
    I18nBehavior,
    settings.RouteObserverBehavior,
  ],

  properties: {
    /** @type {?Map<string, string>} */
    focusConfig_: Object,

    /** @private */
    shouldShowQuickAnswersSettings_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('shouldShowQuickAnswersSettings');
      },
    },

    /** @private Can be disallowed due to flag, policy, locale, etc. */
    isAssistantAllowed_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isAssistantAllowed');
      },
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () =>
          new Set([chromeos.settings.mojom.Setting.kPreferredSearchEngine]),
    },
  },

  /** @override */
  ready() {
    this.focusConfig_ = new Map();
    this.focusConfig_.set(
        settings.routes.SEARCH_SUBPAGE.path, '#searchSubpageTrigger');
    this.focusConfig_.set(
        settings.routes.GOOGLE_ASSISTANT.path, '#assistantSubpageTrigger');
  },

  /**
   * @param {!settings.Route} route
   * @param {!settings.Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== settings.routes.OS_SEARCH) {
      return;
    }

    this.attemptDeepLink();
  },

  /** @private */
  onSearchTap_() {
    settings.Router.getInstance().navigateTo(settings.routes.SEARCH_SUBPAGE);
  },

  /** @private */
  onGoogleAssistantTap_() {
    assert(this.isAssistantAllowed_);
    settings.Router.getInstance().navigateTo(settings.routes.GOOGLE_ASSISTANT);
  },

  /**
   * @param {boolean} toggleValue
   * @return {string}
   * @private
   */
  getAssistantEnabledDisabledLabel_(toggleValue) {
    return this.i18n(
        toggleValue ? 'searchGoogleAssistantEnabled' :
                      'searchGoogleAssistantDisabled');
  },
});
