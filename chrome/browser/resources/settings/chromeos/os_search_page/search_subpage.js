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
    PrefsBehavior,
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
        chromeos.settings.mojom.Setting.kQuickAnswersDefinition,
        chromeos.settings.mojom.Setting.kQuickAnswersTranslation,
        chromeos.settings.mojom.Setting.kQuickAnswersUnitConversion,
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

  /**
   * @private
   */
  onSettingsLinkClick_() {
    Router.getInstance().navigateTo(settings.routes.OS_LANGUAGES_LANGUAGES);
  },

  /**
   * Attaches aria attributes to the translation sub label.
   * @private
   */
  getAriaLabelledTranslationSubLabel_() {
    // Creating a <settings-localized-link> to get aria-labelled content with
    // the link. Since <settings-toggle-button> is a shared element which does
    // not have access to <settings-localized-link> internally, we create dummy
    // element and take its innerHTML here.
    const link = document.createElement('settings-localized-link');
    link.setAttribute(
        'localized-string',
        this.i18nAdvanced('quickAnswersTranslationEnableDescription'));
    link.setAttribute('hidden', true);
    document.body.appendChild(link);
    const innerHTML = link.shadowRoot.querySelector('#container').innerHTML;
    document.body.removeChild(link);
    return innerHTML;
  },
});
