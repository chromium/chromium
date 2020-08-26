// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-languages-page-v2' is the languages sub-page
 * for languages and inputs settings.
 */

Polymer({
  is: 'os-settings-languages-page-v2',

  behaviors: [
    I18nBehavior,
    PrefsBehavior,
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
      notify: true,
    },

    /** @type {!LanguageHelper} */
    languageHelper: Object,

    /**
     * The language to display the details for and its index.
     * @type {{state: !LanguageState, index: number}|undefined}
     * @private
     */
    detailLanguage_: Object,

    /** @private */
    showAddLanguagesDialog_: Boolean,

    /** @private */
    showChangeDeviceLanguageDialog_: {
      type: Boolean,
      value: false,
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
   * @param {string} language
   * @return {string}
   * @private
   */
  getLanguageDisplayName_(language) {
    return this.languageHelper.getLanguage(language).displayName;
  },

  /** @private */
  onChangeSystemLanguageClick_() {
    this.showChangeDeviceLanguageDialog_ = true;
  },

  /** @private */
  onChangeDeviceLanguageDialogClose_() {
    this.showChangeDeviceLanguageDialog_ = false;
    cr.ui.focusWithoutInk(assert(this.$.changeSystemLanguage));
  },

  /**
   * @param {string} language
   * @return {string}
   * @private
   */
  getChangeSystemLanguageButtonDescription_(language) {
    return this.i18n(
        'changeSystemLanguageButtonDescription',
        this.getLanguageDisplayName_(language));
  },

  /**
   * Stamps and opens the Add Languages dialog, registering a listener to
   * disable the dialog's dom-if again on close.
   * @param {!Event} e
   * @private
   */
  onAddLanguagesClick_(e) {
    e.preventDefault();
    this.languagesMetricsProxy_.recordAddLanguages();
    this.showAddLanguagesDialog_ = true;
  },

  /** @private */
  onAddLanguagesDialogClose_() {
    this.showAddLanguagesDialog_ = false;
    cr.ui.focusWithoutInk(assert(this.$.addLanguages));
  },

  /**
   * Checks if there are supported languages that are not enabled but can be
   * enabled.
   * @param {LanguagesModel|undefined} languages
   * @return {boolean} True if there is at least one available language.
   * @private
   */
  canEnableSomeSupportedLanguage_(languages) {
    return languages !== undefined && languages.supported.some(language => {
      return this.languageHelper.canEnableLanguage(language);
    });
  },

  /**
   * @return {boolean} True if the "Move to top" option for |language| should
   *     be visible.
   * @private
   */
  showMoveToTop_() {
    // "Move To Top" is a no-op for the top language.
    return this.detailLanguage_ !== undefined &&
        this.detailLanguage_.index === 0;
  },

  /**
   * @return {boolean} True if the "Move up" option for |language| should
   *     be visible.
   * @private
   */
  showMoveUp_() {
    // "Move up" is a no-op for the top language, and redundant with
    // "Move to top" for the 2nd language.
    return this.detailLanguage_ !== undefined &&
        this.detailLanguage_.index !== 0 && this.detailLanguage_.index !== 1;
  },

  /**
   * @return {boolean} True if the "Move down" option for |language| should be
   *     visible.
   * @private
   */
  showMoveDown_() {
    return this.languages !== undefined && this.detailLanguage_ !== undefined &&
        this.detailLanguage_.index !== this.languages.enabled.length - 1;
  },

  /**
   * Moves the language to the top of the list.
   * @private
   */
  onMoveToTopClick_() {
    /** @type {!CrActionMenuElement} */ (this.$.menu.get()).close();
    this.languageHelper.moveLanguageToFront(
        this.detailLanguage_.state.language.code);
    settings.recordSettingChange();
  },

  /**
   * Moves the language up in the list.
   * @private
   */
  onMoveUpClick_() {
    /** @type {!CrActionMenuElement} */ (this.$.menu.get()).close();
    this.languageHelper.moveLanguage(
        this.detailLanguage_.state.language.code, /*upDirection=*/ true);
    settings.recordSettingChange();
  },

  /**
   * Moves the language down in the list.
   * @private
   */
  onMoveDownClick_() {
    /** @type {!CrActionMenuElement} */ (this.$.menu.get()).close();
    this.languageHelper.moveLanguage(
        this.detailLanguage_.state.language.code, /*upDirection=*/ false);
    settings.recordSettingChange();
  },

  /**
   * Disables the language.
   * @private
   */
  onRemoveLanguageClick_() {
    /** @type {!CrActionMenuElement} */ (this.$.menu.get()).close();
    this.languageHelper.disableLanguage(
        this.detailLanguage_.state.language.code);
    settings.recordSettingChange();
  },

  /**
   * @param {!Event} e
   * @private
   */
  onDotsClick_(e) {
    // Sets a copy of the LanguageState object since it is not data-bound to
    // the languages model directly.
    this.detailLanguage_ =
        /** @type {{state: !LanguageState, index: number}} */ ({
          state: /** @type {!LanguageState} */ (e.model.item),
          index: /** @type {number} */ (e.model.index)
        });

    const menu = /** @type {!CrActionMenuElement} */ (this.$.menu.get());
    menu.showAt(/** @type {!Element} */ (e.target));
  },

  /**
   * @param {!Event} e
   * @private
   */
  onTranslateToggleChange_(e) {
    this.languagesMetricsProxy_.recordToggleTranslate(e.target.checked);
  },
});
