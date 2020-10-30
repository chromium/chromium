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
    prefs: Object,

    /** @private {!SearchEngine} The current selected search engine. */
    currentSearchEngine_: Object,

    /** @private */
    showSearchSelectionDialog_: Boolean,

    /** @type {?Map<string, string>} */
    focusConfig_: Object,

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

  /** @private {?settings.SearchEnginesBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = settings.SearchEnginesBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready() {
    const updateCurrentSearchEngine = searchEngines => {
      this.currentSearchEngine_ =
          searchEngines.defaults.find(searchEngine => searchEngine.default);
    };
    this.browserProxy_.getSearchEnginesList().then(updateCurrentSearchEngine);
    cr.addWebUIListener('search-engines-changed', updateCurrentSearchEngine);

    this.focusConfig_ = new Map();
    if (settings.routes.GOOGLE_ASSISTANT) {
      this.focusConfig_.set(
          settings.routes.GOOGLE_ASSISTANT.path, '#assistantSubpageTrigger');
    }
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
  onDisableExtension_() {
    this.fire('refresh-pref', 'default_search_provider.enabled');
  },

  /** @private */
  onShowSearchSelectionDialogClick_() {
    this.showSearchSelectionDialog_ = true;
  },

  /** @private */
  onSearchSelectionDialogClose_() {
    this.showSearchSelectionDialog_ = false;
    cr.ui.focusWithoutInk(assert(this.$.searchSelectionDialogButton));
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

  /**
   * @param {!chrome.settingsPrivate.PrefObject} pref
   * @return {boolean}
   * @private
   */
  isDefaultSearchControlledByPolicy_(pref) {
    return pref.controlledBy ===
        chrome.settingsPrivate.ControlledBy.USER_POLICY;
  },

  /**
   * @param {!chrome.settingsPrivate.PrefObject} pref
   * @return {boolean}
   * @private
   */
  isDefaultSearchEngineEnforced_(pref) {
    return pref.enforcement === chrome.settingsPrivate.Enforcement.ENFORCED;
  },
});
