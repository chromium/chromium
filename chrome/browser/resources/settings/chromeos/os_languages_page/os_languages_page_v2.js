// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-languages-page-v2' is the languages sub-page
 * for languages and inputs settings.
 */

/**
 * @type {number} Millisecond delay that can be used when closing an action
 * menu to keep it briefly on-screen so users can see the changes.
 */
const kMenuCloseDelay = 100;

Polymer({
  is: 'os-settings-languages-page-v2',

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

    /** @private */
    isGuest_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isGuest');
      },
    },

    /** @private */
    isSecondaryUser_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isSecondaryUser');
      },
    },

    /** @private */
    primaryUserEmail_: {
      type: String,
      value() {
        return loadTimeData.getString('primaryUserEmail');
      },
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kAddLanguage,
        chromeos.settings.mojom.Setting.kChangeDeviceLanguage,
        chromeos.settings.mojom.Setting.kOfferTranslation,
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
    if (route !== settings.routes.OS_LANGUAGES_LANGUAGES) {
      return;
    }

    this.attemptDeepLink();
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
  onChangeDeviceLanguageClick_() {
    this.showChangeDeviceLanguageDialog_ = true;
  },

  /** @private */
  onChangeDeviceLanguageDialogClose_() {
    this.showChangeDeviceLanguageDialog_ = false;
    cr.ui.focusWithoutInk(assert(this.$$('#changeDeviceLanguage')));
  },

  /**
   * @param {string} language
   * @return {string}
   * @private
   */
  getChangeDeviceLanguageButtonDescription_(language) {
    return this.i18n(
        'changeDeviceLanguageButtonDescription',
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
   * @return {boolean} True if the translate checkbox should be disabled.
   * @private
   */
  disableTranslateCheckbox_() {
    if (!this.detailLanguage_ || !this.detailLanguage_.state) {
      return true;
    }

    const languageState = this.detailLanguage_.state;
    if (!languageState.language || !languageState.language.supportsTranslate) {
      return true;
    }

    if (this.languageHelper.isOnlyTranslateBlockedLanguage(languageState)) {
      return true;
    }

    return this.languageHelper.convertLanguageCodeForTranslate(
               languageState.language.code) === this.languages.translateTarget;
  },

  /**
   * Handler for changes to the translate checkbox.
   * @param {!{target: !Element}} e
   * @private
   */
  onTranslateCheckboxChange_(e) {
    if (e.target.checked) {
      this.languageHelper.enableTranslateLanguage(
          this.detailLanguage_.state.language.code);
    } else {
      this.languageHelper.disableTranslateLanguage(
          this.detailLanguage_.state.language.code);
    }
    this.languagesMetricsProxy_.recordTranslateCheckboxChanged(
        e.target.checked);
    settings.recordSettingChange();
    this.closeMenuSoon_();
  },

  /**
   * Closes the shared action menu after a short delay, so when a checkbox is
   * clicked it can be seen to change state before disappearing.
   * @private
   */
  closeMenuSoon_() {
    const menu = /** @type {!CrActionMenuElement} */ (this.$$('#menu').get());
    setTimeout(() => {
      if (menu.open) {
        menu.close();
      }
    }, kMenuCloseDelay);
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

  /**
   * @param {string} languageCode The language code identifying a language.
   * @param {string} translateTarget The translate target language.
   * @return {string} class name for whether it's a translate-target or not.
   * @private
   */
  getTranslationTargetClass_(languageCode, translateTarget) {
    return this.languageHelper.convertLanguageCodeForTranslate(languageCode) ===
            translateTarget ?
        'translate-target' :
        'non-translate-target';
  },
});
