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

    /** @private {!Array<!LanguageState|!SpellCheckLanguageState>|undefined} */
    spellCheckLanguages_: {
      type: Array,
      computed: `getSpellCheckLanguages_(languageSettingsV2Update2Enabled_,
          languages.spellCheckOnLanguages.*, languages.enabled.*)`,
    },

    /** @private */
    showAddInputMethodsDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    showAddSpellcheckLanguagesDialog_: {
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

    /** @private */
    languageSettingsV2Update2Enabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('enableLanguageSettingsV2Update2');
      },
    },

    /**
     * Whether the shortcut reminder for the last used IME is currently showing.
     * @private
     */
    showLastUsedIMEShortcutReminder_: {
      type: Boolean,
      computed: `shouldShowLastUsedIMEShortcutReminder_(
          languages.inputMethods.enabled.length,
          prefs.ash.shortcut_reminders.last_used_ime_dismissed.value)`,
    },

    /**
     * Whether the shortcut reminder for the next IME is currently showing.
     * @private
     */
    showNextIMEShortcutReminder_: {
      type: Boolean,
      computed: `shouldShowNextIMEShortcutReminder_(
          languages.inputMethods.enabled.length,
          prefs.ash.shortcut_reminders.next_ime_dismissed.value)`,
    },

    /**
     * The body of the currently showing shortcut reminders.
     * @private {!Array<string>}
     */
    shortcutReminderBody_: {
      type: Array,
      computed: `getShortcutReminderBody_(showLastUsedIMEShortcutReminder_,
          showNextIMEShortcutReminder_)`,
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

  /** @private */
  onAddSpellcheckLanguagesClick_() {
    this.showAddSpellcheckLanguagesDialog_ = true;
  },

  /** @private */
  onAddSpellcheckLanguagesDialogClose_() {
    this.showAddSpellcheckLanguagesDialog_ = false;

    if (this.languages.spellCheckOnLanguages.length === 0) {
      // User closed the dialog right after turning on spell check without any
      // existing spell check languages - turn off spell checking if this is
      // the case.
      this.setPrefValue('browser.enable_spellchecking', false);
    }

    // Because #addSpellcheckLanguages is not statically created (as it is
    // within a <template is="dom-if">), we need to use
    // this.$$("#addSpellcheckLanguages") instead of
    // this.$.addSpellCheckLanguages.
    cr.ui.focusWithoutInk(assert(this.$$('#addSpellcheckLanguages')));
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
        inputMethod => inputMethod.id !== targetInputMethod.id &&
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
   * @param {!SpellCheckLanguageState} lang
   * @private
   */
  getRemoveSpellcheckLanguageTooltip_(lang) {
    return this.i18n(
        'removeSpellCheckLanguageTooltip', lang.language.displayName);
  },

  /**
   * @param {!{model: !{item: SpellCheckLanguageState}}} e
   * @private
   */
  onRemoveSpellcheckLanguageClick_(e) {
    this.languageHelper.toggleSpellCheck(e.model.item.language.code, false);
    settings.recordSettingChange();
  },

  /**
   * @return {string|undefined}
   * @param {!Event} e
   * @private
   */
  onSpellcheckToggleChange_(e) {
    this.languagesMetricsProxy_.recordToggleSpellCheck(e.target.checked);

    if (this.languageSettingsV2Update2Enabled_ && e.target.checked &&
        this.languages.spellCheckOnLanguages.length === 0) {
      // In LSV2 Update 2, we never want to enable spell check without the user
      // having a spell check language. When this happens, we try estimating
      // their expected spell check language (their device language, assuming
      // that the user has an input method which supports that language).
      // If that doesn't work, we fall back on prompting the user to enable a
      // spell check language. If the user dismisses this dialog without adding
      // a spell check language, we disable spell check again
      // (see |onAddSpellcheckLanguagesDialogClose_|).

      // This assert is safe as prospectiveUILanguage is always defined in
      // languages.js' |createModel_()|.
      const deviceLanguageCode = assert(this.languages.prospectiveUILanguage);
      // However, deviceLanguage itself may be undefined as it is possible that
      // it was set outside of CrOS language settings (normally when debugging
      // or in tests).
      const deviceLanguage =
          this.languageHelper.getLanguage(deviceLanguageCode);
      if (deviceLanguage && deviceLanguage.supportsSpellcheck &&
          this.languages.inputMethods.enabled.some(
              inputMethod =>
                  inputMethod.languageCodes.includes(deviceLanguageCode))) {
        this.languageHelper.toggleSpellCheck(deviceLanguageCode, true);
      } else {
        this.onAddSpellcheckLanguagesClick_();
      }
    }
  },

  /**
   * Returns the value to use as the |pref| attribute for the policy indicator
   * of spellcheck languages, based on whether or not the language is enabled.
   * @param {boolean} isEnabled Whether the language is enabled or not.
   * @private
   */
  getIndicatorPrefForManagedSpellcheckLanguage_(isEnabled) {
    return isEnabled ? this.getPref('spellcheck.forced_dictionaries') :
                       this.getPref('spellcheck.blocked_dictionaries');
  },

  /**
   * Returns an array of spell check languages for the UI.
   * @return {!Array<!LanguageState|!SpellCheckLanguageState>|undefined}
   * @private
   */
  getSpellCheckLanguages_() {
    if (this.languages === undefined) {
      return undefined;
    }
    if (this.languageSettingsV2Update2Enabled_) {
      const languages = [...this.languages.spellCheckOnLanguages];
      languages.sort(
          (a, b) =>
              a.language.displayName.localeCompare(b.language.displayName));
      return languages;
    }
    const combinedLanguages =
        this.languages.enabled.concat(this.languages.spellCheckOnLanguages);
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
   * @param {!LanguageState|!SpellCheckLanguageState} item
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
  },

  /**
   * Gets the appropriate CSS class for the Enhanced spell check toggle
   * depending on whether Update 2 is enabled or not.
   * @private
   */
  getEnhancedSpellCheckClass_() {
    return this.languageSettingsV2Update2Enabled_ ? '' : 'hr';
  },

  /**
   * @private
   */
  isEnableSpellcheckingDisabled_() {
    return !this.languageSettingsV2Update2Enabled_ &&
        (this.spellCheckLanguages_ && this.spellCheckLanguages_.length === 0);
  },

  /**
   * @param {boolean} update2Enabled
   * @param {boolean} spellCheckOn
   * @return {boolean}
   */
  isCollapseOpened_(update2Enabled, spellCheckOn) {
    return !update2Enabled || spellCheckOn;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowLastUsedIMEShortcutReminder_() {
    // User has already dismissed the shortcut reminder.
    if (this.getPref('ash.shortcut_reminders.last_used_ime_dismissed').value) {
      return false;
    }
    // Need at least 2 input methods to be shown the reminder.
    return !!this.languages && this.languages.inputMethods.enabled.length >= 2;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowNextIMEShortcutReminder_() {
    // User has already dismissed the shortcut reminder.
    if (this.getPref('ash.shortcut_reminders.next_ime_dismissed').value) {
      return false;
    }
    // Need at least 3 input methods to be shown the reminder.
    return !!this.languages && this.languages.inputMethods.enabled.length >= 3;
  },

  /**
   * @return {!Array<string>}
   * @private
   */
  getShortcutReminderBody_() {
    const /** !Array<string> */ reminderBody = [];
    if (this.showLastUsedIMEShortcutReminder_) {
      reminderBody.push(this.i18nAdvanced('imeShortcutReminderLastUsed'));
    }
    if (this.showNextIMEShortcutReminder_) {
      reminderBody.push(this.i18nAdvanced('imeShortcutReminderNext'));
    }
    return reminderBody;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowShortcutReminder_() {
    return this.languageSettingsV2Update2Enabled_ &&
        this.shortcutReminderBody_ && this.shortcutReminderBody_.length > 0;
  },

  /** @private */
  onShortcutReminderDismiss_() {
    if (this.showLastUsedIMEShortcutReminder_) {
      this.setPrefValue('ash.shortcut_reminders.last_used_ime_dismissed', true);
    }
    if (this.showNextIMEShortcutReminder_) {
      this.setPrefValue('ash.shortcut_reminders.next_ime_dismissed', true);
    }
  },
});
