// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-input-page' is the input sub-page
 * for language and input method settings.
 */
Polymer({
  is: 'os-settings-input-page',

  behaviors: [
    DeepLinkingBehavior,
    I18nBehavior,
    PrefsBehavior,
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

    /**
     * Read-only reference to the languages model provided by the
     * 'os-settings-languages' instance.
     * @type {!LanguagesModel|undefined}
     */
    languages: {
      type: Object,
    },

    /** @type {!LanguageHelper} */
    languageHelper: Object,

    /**
     * @private {!Array<!LanguageState|!ForcedLanguageState>}
     */
    spellCheckLanguages_: {
      type: Array,
      value() {
        return [];
      },
    },

    /** @private */
    showAddInputMethodsDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private {!chrome.languageSettingsPrivate.InputMethod} */
    inputMethodToRemove_: Object,

    /** @private */
    showRemoveInputMethodDialog_: {
      type: Boolean,
      value: false,
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kShowInputOptionsInShelf,
        chromeos.settings.mojom.Setting.kAddInputMethod,
        chromeos.settings.mojom.Setting.kSpellCheck,
      ]),
    },
  },

  /** @private {?settings.LanguagesMetricsProxy} */
  languagesMetricsProxy_: null,

  /** @override */
  created() {
    this.languagesMetricsProxy_ =
        settings.LanguagesMetricsProxyImpl.getInstance();
  },

  /**
   * @param {!settings.Route} route
   * @param {!settings.Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== settings.routes.OS_LANGUAGES_INPUT) {
      return;
    }

    this.attemptDeepLink();
  },

  /**
   * @param {!Event} e
   * @private
   */
  onShowImeMenuChange_(e) {
    this.languagesMetricsProxy_.recordToggleShowInputOptionsOnShelf(
        e.target.checked);
  },

  /**
   * Handler for click events on an input method on the main page,
   * which sets it as the current input method.
   * @param {!{model: !{item: !chrome.languageSettingsPrivate.InputMethod},
   *           target: !{tagName: string}}} e
   * @private
   */
  onInputMethodClick_(e) {
    // Clicks on the button are handled in onInputMethodOptionsClick_.
    if (e.target.tagName === 'CR-ICON-BUTTON') {
      return;
    }

    this.languageHelper.setCurrentInputMethod(e.model.item.id);
  },

  /**
   * Handler for <Enter> events on an input method on the main page,
   * which sets it as the current input method.
   * @param {!{model: !{item: !chrome.languageSettingsPrivate.InputMethod},
   *           key: string}} e
   * @private
   */
  onInputMethodKeyPress_(e) {
    // Ignores key presses other than <Enter>.
    if (e.key !== 'Enter') {
      return;
    }

    this.languageHelper.setCurrentInputMethod(e.model.item.id);
  },

  /**
   * Opens the input method extension's options page in a new tab (or focuses
   * an existing instance of the IME's options).
   * @param {!{model: !{item: chrome.languageSettingsPrivate.InputMethod}}} e
   * @private
   */
  openExtensionOptionsPage_(e) {
    this.languageHelper.openInputMethodOptions(e.model.item.id);
  },


  /**
   * @param {string} id The input method ID.
   * @return {boolean} True if there is a options page in ChromeOS settings
   *     for the input method ID.
   * @private
   */
  hasOptionsPageInSettings_(id) {
    return loadTimeData.getBoolean('imeOptionsInSettings') &&
        settings.input_method_util.hasOptionsPageInSettings(id);
  },

  /**
   * @param {!{model: !{item: chrome.languageSettingsPrivate.InputMethod}}} e
   * @private
   */
  navigateToOptionsPageInSettings_(e) {
    const params = new URLSearchParams;
    params.append('id', e.model.item.id);
    settings.Router.getInstance().navigateTo(
        settings.routes.OS_LANGUAGES_INPUT_METHOD_OPTIONS, params);
  },

  /**
   * @param {string} id The input method ID.
   * @param {string} currentId The ID of the currently enabled input method.
   * @return {boolean} True if the IDs match.
   * @private
   */
  isCurrentInputMethod_(id, currentId) {
    return id === currentId;
  },

  /**
   * @param {string} id The input method ID.
   * @param {string} currentId The ID of the currently enabled input method.
   * @return {string} The class for the input method item.
   * @private
   */
  getInputMethodItemClass_(id, currentId) {
    return this.isCurrentInputMethod_(id, currentId) ? 'selected' : '';
  },

  /**
   * @param {string} id The selected input method ID.
   * @param {string} currentId The ID of the currently enabled input method.
   * @return {string} The default tab index '0' if the selected input method
   *     is not currently enabled; otherwise, returns an empty string which
   *     effectively unsets the tabindex attribute.
   * @private
   */
  getInputMethodTabIndex_(id, currentId) {
    return id === currentId ? '' : '0';
  },

  /**
   * @param {string} inputMethodName
   * @return {string}
   * @private
   */
  getOpenOptionsPageLabel_(inputMethodName) {
    return this.i18n('openOptionsPage', inputMethodName);
  },

  /** @private */
  onAddInputMethodClick_() {
    this.languagesMetricsProxy_.recordAddInputMethod();
    this.showAddInputMethodsDialog_ = true;
  },

  /** @private */
  onAddInputMethodsDialogClose_() {
    this.showAddInputMethodsDialog_ = false;
    cr.ui.focusWithoutInk(assert(this.$.addInputMethod));
  },

  /**
   * @param {!chrome.languageSettingsPrivate.InputMethod} inputMethod
   * @private
   */
  getRemoveInputMethodTooltip_(inputMethod) {
    return this.i18n('removeInputMethodLabel', inputMethod.displayName);
  },

  /**
   * @param {!{model: !{item: chrome.languageSettingsPrivate.InputMethod}}} e
   * @private
   */
  onRemoveInputMethodClick_(e) {
    this.inputMethodToRemove_ = e.model.item;
    this.showRemoveInputMethodDialog_ = true;
  },

  /** @private */
  onRemoveInputMethodDialogClose_() {
    this.showRemoveInputMethodDialog_ = false;
  },

  /**
   * @return {string|undefined}
   * @private
   */
  getSpellCheckSubLabel_() {
    return this.spellCheckLanguages_.length ?
        undefined :  // equivalent to not setting the sublabel in the HTML.
        this.i18n('spellCheckDisabledReason');
  },

  /**
   * @param {!Event} e
   * @private
   */
  onSpellcheckToggleChange_(e) {
    this.languagesMetricsProxy_.recordToggleSpellCheck(e.target.checked);
  },
});
