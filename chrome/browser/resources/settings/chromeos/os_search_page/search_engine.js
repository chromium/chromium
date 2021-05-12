// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-search-engine' is the settings module for setting
 * the preferred search engine.
 */
Polymer({
  is: 'settings-search-engine',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    prefs: Object,

    /** @private {!SearchEngine} The current selected search engine. */
    currentSearchEngine_: Object,

    /** @private */
    showSearchSelectionDialog_: Boolean,
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
  },

  /** @override */
  focus() {
    this.$$('#searchSelectionDialogButton').focus();
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
    cr.ui.focusWithoutInk(assert(this.$$('#searchSelectionDialogButton')));
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
