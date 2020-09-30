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
    /* Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @type {!Map<string, (string|Function)>} */
    focusConfig: {
      type: Object,
      observer: 'focusConfigChanged_',
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

    /** @private {!Array<!LanguageState|!ForcedLanguageState>|undefined} */
    spellCheckLanguages_: {
      type: Array,
      computed:
          'getSpellCheckLanguages_(languages.enabled.*, languages.forcedSpellCheckLanguages.*)',
    },

    /** @private */
    showAddInputMethodsDialog_: {
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

  observers: [
    'updateSpellcheckPref_(spellCheckLanguages_)',
  ],

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
   * @param {!Map<string, (string|Function)>} newConfig
   * @param {?Map<string, (string|Function)>} oldConfig
   * @private
   */
  focusConfigChanged_(newConfig, oldConfig) {
    // focusConfig is set only once on the parent, so this observer should
    // only fire once.
    assert(!oldConfig);
    this.focusConfig.set(
        settings.routes.OS_LANGUAGES_EDIT_DICTIONARY.path,
        () => cr.ui.focusWithoutInk(this.$.editDictionarySubpageTrigger));
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
   * @return {boolean}
   * @private
   */
  inputMethodsLimitedByPolicy_() {
    const allowedInputMethodsPref =
        this.getPref('settings.language.allowed_input_methods');
    return !!allowedInputMethodsPref && allowedInputMethodsPref.value.length;
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
    this.languagesMetricsProxy_.recordInteraction(
        settings.LanguagesPageInteraction.SWITCH_INPUT_METHOD);
    settings.recordSettingChange();
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
   * @param {!chrome.languageSettingsPrivate.InputMethod} targetInputMethod
   * @private
   */
  disableRemoveInputMethod_(targetInputMethod) {
    // Third-party IMEs can always be removed.
    if (!this.languageHelper.isComponentIme(targetInputMethod)) {
      return false;
    }

    // Disable remove if there is no other component IME (pre-installed
    // system IMES) enabled.
    return !this.languages.inputMethods.enabled.some(
        inputMethod => inputMethod.id != targetInputMethod.id &&
            this.languageHelper.isComponentIme(inputMethod));
  },

  /**
   * @param {!chrome.languageSettingsPrivate.InputMethod} inputMethod
   * @private
   */
  getRemoveInputMethodTooltip_(inputMethod) {
    return this.i18n('removeInputMethodTooltip', inputMethod.displayName);
  },

  /**
   * @param {!{model: !{item: chrome.languageSettingsPrivate.InputMethod}}} e
   * @private
   */
  onRemoveInputMethodClick_(e) {
    this.languageHelper.removeInputMethod(e.model.item.id);
    settings.recordSettingChange();
  },

  /**
   * @return {string|undefined}
   * @param {!Event} e
   * @private
   */
  onSpellcheckToggleChange_(e) {
    this.languagesMetricsProxy_.recordToggleSpellCheck(e.target.checked);
  },

  /**
   * Returns the value to use as the |pref| attribute for the policy indicator
   * of spellcheck languages, based on whether or not the language is enabled.
   * @param {boolean} isEnabled Whether the language is enabled or not.
   * @private
   */
  getIndicatorPrefForManagedSpellcheckLanguage_(isEnabled) {
    return isEnabled ? this.getPref('spellcheck.forced_dictionaries') :
                       this.getPref('spellcheck.blacklisted_dictionaries');
  },

  /**
   * Returns an array of enabled languages that support spell check, plus
   * spellcheck languages that are force-enabled by policy.
   * @return {!Array<!LanguageState|!ForcedLanguageState>|undefined}
   * @private
   */
  getSpellCheckLanguages_() {
    if (this.languages === undefined) {
      return undefined;
    }
    const combinedLanguages =
        this.languages.enabled.concat(this.languages.forcedSpellCheckLanguages);
    const supportedSpellcheckLanguagesSet = new Set();
    const supportedSpellcheckLanguages = [];

    combinedLanguages.forEach(languageState => {
      if (!supportedSpellcheckLanguagesSet.has(languageState.language.code) &&
          languageState.language.supportsSpellcheck) {
        supportedSpellcheckLanguages.push(languageState);
        supportedSpellcheckLanguagesSet.add(languageState.language.code);
      }
    });

    return supportedSpellcheckLanguages;
  },

  /** @private */
  updateSpellcheckPref_() {
    if (this.spellCheckLanguages_ === undefined) {
      return;
    }

    // TODO(crbug/1126239): Investigate feasibility of moving this pref update
    // to spellcheck_service.
    if (this.spellCheckLanguages_.length === 0) {
      // If there are no supported spell check languages, automatically turn
      // off spell check to indicate no spell check will happen.
      this.setPrefValue('browser.enable_spellchecking', false);
    }
  },

  /**
   * Handler for enabling or disabling spell check for a specific language.
   * @param {!{target: Element, model: !{item: !LanguageState}}} e
   * @private
   */
  onSpellCheckLanguageChange_(e) {
    const item = e.model.item;
    if (!item.language.supportsSpellcheck) {
      return;
    }

    this.languageHelper.toggleSpellCheck(
        item.language.code, !item.spellCheckEnabled);
  },

  /**
   * Handler for clicking on the name of the language. The action taken must
   * match the control that is available.
   * @param {!{target: Element, model: !{item: !LanguageState}}} e
   * @private
   */
  onSpellCheckNameClick_(e) {
    assert(!this.isSpellCheckNameClickDisabled_(e.model.item));
    this.onSpellCheckLanguageChange_(e);
  },

  /**
   * Name only supports clicking when language is not managed, supports
   * spellcheck, and the dictionary has been downloaded with no errors.
   * @param {!LanguageState|!ForcedLanguageState} item
   * @return {boolean}
   * @private
   */
  isSpellCheckNameClickDisabled_(item) {
    return item.isManaged || item.downloadDictionaryFailureCount > 0 ||
        !this.getPref('browser.enable_spellchecking').value;
  },

  /**
   * Handler to initiate another attempt at downloading the spell check
   * dictionary for a specified language.
   * @param {!{target: Element, model: !{item: !LanguageState}}} e
   * @private
   */
  onRetryDictionaryDownloadClick_(e) {
    assert(e.model.item.downloadDictionaryFailureCount > 0);
    this.languageHelper.retryDownloadDictionary(e.model.item.language.code);
  },

  /**
   * @param {!LanguageState} item
   * @return {!string}
   * @private
   */
  getDictionaryDownloadRetryAriaLabel_(item) {
    return this.i18n(
        'languagesDictionaryDownloadRetryDescription',
        item.language.displayName);
  },

  /**
   * Opens the Custom Dictionary page.
   * @private
   */
  onEditDictionaryClick_() {
    this.languagesMetricsProxy_.recordInteraction(
        settings.LanguagesPageInteraction.OPEN_CUSTOM_SPELL_CHECK);
    settings.Router.getInstance().navigateTo(
        settings.routes.OS_LANGUAGES_EDIT_DICTIONARY);
  }
});
