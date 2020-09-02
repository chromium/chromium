// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-languages-page' is the settings sub-page
 * for language and input method settings.
 */
cr.define('settings', function() {
  /**
   * @type {number} Millisecond delay that can be used when closing an action
   *      menu to keep it briefly on-screen.
   */
  const kMenuCloseDelay = 100;

  Polymer({
    is: 'os-settings-languages-page',

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
       * The language to display the details for.
       * @type {!LanguageState|undefined}
       * @private
       */
      detailLanguage_: Object,

      /** @private */
      showAddLanguagesDialog_: Boolean,

      /** @type {!Map<string, (string|Function)>} */
      focusConfig: {
        type: Object,
        observer: 'focusConfigChanged_',
      },

      /** @private */
      isGuest_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isGuest');
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
          chromeos.settings.mojom.Setting.kShowInputOptionsInShelf,
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
      if (route !== settings.routes.OS_LANGUAGES_DETAILS) {
        return;
      }

      this.attemptDeepLink();
    },

    /** @private {boolean} */
    isChangeInProgress_: false,

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
          settings.routes.OS_LANGUAGES_INPUT_METHODS.path,
          () => cr.ui.focusWithoutInk(this.$.manageInputMethods));
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
     * Stamps and opens the Add Languages dialog, registering a listener to
     * disable the dialog's dom-if again on close.
     * @param {!Event} e
     * @private
     */
    onAddLanguagesTap_(e) {
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
      return languages == undefined || languages.supported.some(language => {
        return this.languageHelper.canEnableLanguage(language);
      });
    },

    /**
     * Used to determine whether to show the separator between checkbox settings
     * and move buttons in the dialog menu.
     * @return {boolean} True if there is currently more than one selected
     *     language.
     * @private
     */
    shouldShowDialogSeparator_() {
      return this.languages != undefined && this.languages.enabled.length > 1 &&
          !this.isGuest_;
    },

    /**
     * Used to determine which "Move" buttons to show for ordering enabled
     * languages.
     * @param {number} n
     * @return {boolean} True if |language| is at the |n|th index in the list of
     *     enabled languages.
     * @private
     */
    isNthLanguage_(n) {
      if (this.languages == undefined || this.detailLanguage_ == undefined) {
        return false;
      }

      if (n >= this.languages.enabled.length) {
        return false;
      }

      const compareLanguage = assert(this.languages.enabled[n]);
      return this.detailLanguage_.language == compareLanguage.language;
    },

    /**
     * @return {boolean} True if the "Move to top" option for |language| should
     *     be visible.
     * @private
     */
    showMoveUp_() {
      // "Move up" is a no-op for the top language, and redundant with
      // "Move to top" for the 2nd language.
      return !this.isNthLanguage_(0) && !this.isNthLanguage_(1);
    },

    /**
     * @return {boolean} True if the "Move down" option for |language| should be
     *     visible.
     * @private
     */
    showMoveDown_() {
      return this.languages != undefined &&
          !this.isNthLanguage_(this.languages.enabled.length - 1);
    },

    /**
     * @param {!Object} change Polymer change object for languages.enabled.*.
     * @return {boolean} True if there are less than 2 languages.
     */
    isHelpTextHidden_(change) {
      return this.languages != undefined && this.languages.enabled.length <= 1;
    },

    /**
     * Opens the Manage Input Methods page.
     * @private
     */
    onManageInputMethodsTap_() {
      this.languagesMetricsProxy_.recordManageInputMethods();
      settings.Router.getInstance().navigateTo(
          settings.routes.OS_LANGUAGES_INPUT_METHODS);
    },

    /**
     * Handler for tap and <Enter> events on an input method on the main page,
     * which sets it as the current input method.
     * @param {!{model: !{item: !chrome.languageSettingsPrivate.InputMethod},
     *           target: !{tagName: string},
     *           type: string,
     *           key: (string|undefined)}} e
     */
    onInputMethodTap_(e) {
      // Taps on the button are handled in onInputMethodOptionsTap_.
      // TODO(dschuyler): The row has two operations that are not clearly
      // delineated. crbug.com/740691
      if (e.target.tagName == 'CR-ICON-BUTTON') {
        return;
      }

      // Ignore key presses other than <Enter>.
      if (e.type == 'keypress' && e.key != 'Enter') {
        return;
      }

      // Set the input method.
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
     * @param {string} id Input method ID.
     * @return {boolean} True if there is a options page in ChromeOS settings
     *     for the input method ID.
     * @private
     */
    hasOptionsPageInSettings_(id) {
      return loadTimeData.getBoolean('imeOptionsInSettings') &&
          settings.input_method_util.hasOptionsPageInSettings(id);
    },

    /**
     * Navigate to the input method options page in ChromeOS settings.
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
     * @return {boolean} True for a secondary user in a multi-profile session.
     * @private
     */
    isSecondaryUser_() {
      return loadTimeData.getBoolean('isSecondaryUser');
    },

    /**
     * @param {string} languageCode The language code identifying a language.
     * @param {string} prospectiveUILanguage The prospective UI language.
     * @return {boolean} True if the prospective UI language is set to
     *     |languageCode| but requires a restart to take effect.
     * @private
     */
    isRestartRequired_(languageCode, prospectiveUILanguage) {
      return prospectiveUILanguage == languageCode &&
          this.languageHelper.requiresRestart();
    },

    /** @private */
    onCloseMenu_() {
      if (!this.isChangeInProgress_) {
        return;
      }
      Polymer.dom.flush();
      this.isChangeInProgress_ = false;
      const restartButton = this.$$('#restartButton');
      if (!restartButton) {
        return;
      }
      cr.ui.focusWithoutInk(restartButton);
    },

    /**
     * @param {!LanguageState} languageState
     * @param {string} prospectiveUILanguage The chosen UI language.
     * @return {boolean} True if the given language cannot be set as the
     *     prospective UI language by the user.
     * @private
     */
    disableUILanguageCheckbox_(languageState, prospectiveUILanguage) {
      if (this.detailLanguage_ === undefined) {
        return true;
      }

      // UI language setting belongs to the primary user.
      if (this.isSecondaryUser_()) {
        return true;
      }

      // If the language cannot be a UI language, we can't set it as the
      // prospective UI language.
      if (!languageState.language.supportsUI) {
        return true;
      }

      // Unchecking the currently chosen language doesn't make much sense.
      if (languageState.language.code == prospectiveUILanguage) {
        return true;
      }

      // Check if the language is prohibited by the current "AllowedLanguages"
      // policy.
      if (languageState.language.isProhibitedLanguage) {
        return true;
      }

      // Otherwise, the prospective language can be changed to this language.
      return false;
    },

    /**
     * Handler for changes to the UI language checkbox.
     * @param {!{target: !Element}} e
     * @private
     */
    onUILanguageChange_(e) {
      // We don't support unchecking this checkbox. TODO(michaelpg): Ask for a
      // simpler widget.
      assert(e.target.checked);
      this.languagesMetricsProxy_.recordInteraction(
          settings.LanguagesPageInteraction.SWITCH_SYSTEM_LANGUAGE);
      this.isChangeInProgress_ = true;
      this.languageHelper.setProspectiveUILanguage(
          this.detailLanguage_.language.code);
      this.languageHelper.moveLanguageToFront(
          this.detailLanguage_.language.code);

      this.closeMenuSoon_();
    },

    /**
     * Moves the language to the top of the list.
     * @private
     */
    onMoveToTopTap_() {
      /** @type {!CrActionMenuElement} */ (this.$.menu.get()).close();
      this.languageHelper.moveLanguageToFront(
          this.detailLanguage_.language.code);
      settings.recordSettingChange();
    },

    /**
     * Moves the language up in the list.
     * @private
     */
    onMoveUpTap_() {
      /** @type {!CrActionMenuElement} */ (this.$.menu.get()).close();
      this.languageHelper.moveLanguage(
          this.detailLanguage_.language.code, true /* upDirection */);
      settings.recordSettingChange();
    },

    /**
     * Moves the language down in the list.
     * @private
     */
    onMoveDownTap_() {
      /** @type {!CrActionMenuElement} */ (this.$.menu.get()).close();
      this.languageHelper.moveLanguage(
          this.detailLanguage_.language.code, false /* upDirection */);
      settings.recordSettingChange();
    },

    /**
     * Disables the language.
     * @private
     */
    onRemoveLanguageTap_() {
      /** @type {!CrActionMenuElement} */ (this.$.menu.get()).close();
      this.languageHelper.disableLanguage(this.detailLanguage_.language.code);
      settings.recordSettingChange();
    },

    /**
     * Checks whether the prospective UI language (the pref that indicates what
     * language to use in Chrome) matches the current language.
     * @param {string} languageCode The language code identifying a language.
     * @param {string} prospectiveUILanguage The prospective UI language.
     * @return {boolean} True if the given language matches the prospective UI
     *     pref (which may be different from the actual UI language).
     * @private
     */
    isProspectiveUILanguage_(languageCode, prospectiveUILanguage) {
      return languageCode == prospectiveUILanguage;
    },

    /**
     * Returns either the "selected" class, if the language matches the
     * prospective UI language, or an empty string.
     * @param {string} languageCode The language code identifying a language.
     * @param {string} prospectiveUILanguage The prospective UI language.
     * @return {string} The class name for the language item.
     * @private
     */
    getLanguageItemClass_(languageCode, prospectiveUILanguage) {
      if (languageCode == prospectiveUILanguage) {
        return 'selected';
      }
      return '';
    },

    /**
     * @param {string} id The input method ID.
     * @param {string} currentId The ID of the currently enabled input method.
     * @return {boolean} True if the IDs match.
     * @private
     */
    isCurrentInputMethod_(id, currentId) {
      return id == currentId;
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
     * @param {!Event} e
     * @private
     */
    onDotsTap_(e) {
      // Set a copy of the LanguageState object since it is not data-bound to
      // the languages model directly.
      this.detailLanguage_ = /** @type {!LanguageState} */ (Object.assign(
          {},
          /** @type {!{model: !{item: !LanguageState}}} */ (e).model.item));

      // Ensure the template has been stamped.
      let menu =
          /** @type {?CrActionMenuElement} */ (this.$.menu.getIfExists());
      if (!menu) {
        menu = /** @type {!CrActionMenuElement} */ (this.$.menu.get());

        // In a CrOS multi-user session, the primary user controls the UI
        // language.
        // TODO(michaelpg): The language selection should not be hidden, but
        // should show a policy indicator. crbug.com/648498
        if (this.isSecondaryUser_()) {
          menu.querySelector('#uiLanguageItem').hidden = true;
        }
      }

      menu.showAt(/** @type {!Element} */ (e.target));
    },

    /**
     * Closes the shared action menu after a short delay, so when a checkbox is
     * clicked it can be seen to change state before disappearing.
     * @private
     */
    closeMenuSoon_() {
      const menu = /** @type {!CrActionMenuElement} */ (this.$.menu.get());
      setTimeout(function() {
        if (menu.open) {
          menu.close();
        }
      }, settings.kMenuCloseDelay);
    },

    /**
     * Handler for the restart button.
     * @private
     */
    onRestartTap_() {
      settings.recordSettingChange();
      this.languagesMetricsProxy_.recordInteraction(
          settings.LanguagesPageInteraction.RESTART);
      settings.LifetimeBrowserProxyImpl.getInstance().signOutAndRestart();
    },

    /**
     * Toggles the expand button within the element being listened to.
     * @param {!Event} e
     * @private
     */
    toggleExpandButton_(e) {
      // The expand button handles toggling itself.
      const expandButtonTag = 'CR-EXPAND-BUTTON';
      if (e.target.tagName == expandButtonTag) {
        return;
      }

      if (!e.currentTarget.hasAttribute('actionable')) {
        return;
      }

      /** @type {!CrExpandButtonElement} */
      const expandButton = e.currentTarget.querySelector(expandButtonTag);
      assert(expandButton);
      expandButton.expanded = !expandButton.expanded;
      cr.ui.focusWithoutInk(expandButton);
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
      return id == currentId ? '' : '0';
    },

    /**
     * Handles the mousedown even by preventing focusing an input method list
     * item. This is only registered by the input method list item to avoid
     * unwanted focus.
     * @param {!Event} e
     * @private
     */
    onMouseDown_(e) {
      // Preventing the mousedown event from propagating prevents focus being
      // set.
      e.preventDefault();
    },

    /**
     * @param {string} language The language displayed in the row
     * @return {string}
     * @private
     */
    getRestartButtonDescription_(language) {
      return this.i18n('displayLanguageRestart', language);
    },
  });
  // #cr_define_end
  return {kMenuCloseDelay};
});
