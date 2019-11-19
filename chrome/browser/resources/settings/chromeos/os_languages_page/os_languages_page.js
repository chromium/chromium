// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-languages-page' is the settings sub-page
 * for language and input method settings.
 */
cr.exportPath('settings');

/**
 * @type {number} Millisecond delay that can be used when closing an action
 *      menu to keep it briefly on-screen.
 */
settings.kMenuCloseDelay = 100;

(function() {
'use strict';

Polymer({
  is: 'os-settings-languages-page',

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
      value: function() {
        return loadTimeData.getBoolean('isGuest');
      },
    },
  },

  /** @private {boolean} */
  isChangeInProgress_: false,

  /**
   * @param {!Map<string, (string|Function)>} newConfig
   * @param {?Map<string, (string|Function)>} oldConfig
   * @private
   */
  focusConfigChanged_: function(newConfig, oldConfig) {
    // focusConfig is set only once on the parent, so this observer should only
    // fire once.
    assert(!oldConfig);
    this.focusConfig.set(
        settings.routes.INPUT_METHODS.path,
        () => cr.ui.focusWithoutInk(this.$.manageInputMethods));
  },

  /**
   * Stamps and opens the Add Languages dialog, registering a listener to
   * disable the dialog's dom-if again on close.
   * @param {!Event} e
   * @private
   */
  onAddLanguagesTap_: function(e) {
    e.preventDefault();
    this.showAddLanguagesDialog_ = true;
  },

  /** @private */
  onAddLanguagesDialogClose_: function() {
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
  canEnableSomeSupportedLanguage_: function(languages) {
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
  shouldShowDialogSeparator_: function() {
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
  isNthLanguage_: function(n) {
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
   * @return {boolean} True if the "Move to top" option for |language| should be
   *     visible.
   * @private
   */
  showMoveUp_: function() {
    // "Move up" is a no-op for the top language, and redundant with
    // "Move to top" for the 2nd language.
    return !this.isNthLanguage_(0) && !this.isNthLanguage_(1);
  },

  /**
   * @return {boolean} True if the "Move down" option for |language| should be
   *     visible.
   * @private
   */
  showMoveDown_: function() {
    return this.languages != undefined &&
        !this.isNthLanguage_(this.languages.enabled.length - 1);
  },

  /**
   * @param {!Object} change Polymer change object for languages.enabled.*.
   * @return {boolean} True if there are less than 2 languages.
   */
  isHelpTextHidden_: function(change) {
    return this.languages != undefined && this.languages.enabled.length <= 1;
  },

  /**
   * Opens the Manage Input Methods page.
   * @private
   */
  onManageInputMethodsTap_: function() {
    settings.navigateTo(settings.routes.INPUT_METHODS);
  },

  /**
   * Handler for tap and <Enter> events on an input method on the main page,
   * which sets it as the current input method.
   * @param {!{model: !{item: !chrome.languageSettingsPrivate.InputMethod},
   *           target: !{tagName: string},
   *           type: string,
   *           key: (string|undefined)}} e
   */
  onInputMethodTap_: function(e) {
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
  onInputMethodOptionsTap_: function(e) {
    this.languageHelper.openInputMethodOptions(e.model.item.id);
  },

  /**
   * @return {boolean} True for a secondary user in a multi-profile session.
   * @private
   */
  isSecondaryUser_: function() {
    return loadTimeData.getBoolean('isSecondaryUser');
  },

  /**
   * @param {string} languageCode The language code identifying a language.
   * @param {string} prospectiveUILanguage The prospective UI language.
   * @return {boolean} True if the prospective UI language is set to
   *     |languageCode| but requires a restart to take effect.
   * @private
   */
  isRestartRequired_: function(languageCode, prospectiveUILanguage) {
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
  disableUILanguageCheckbox_: function(languageState, prospectiveUILanguage) {
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
  onUILanguageChange_: function(e) {
    // We don't support unchecking this checkbox. TODO(michaelpg): Ask for a
    // simpler widget.
    assert(e.target.checked);
    this.isChangeInProgress_ = true;
    this.languageHelper.setProspectiveUILanguage(
        this.detailLanguage_.language.code);
    this.languageHelper.moveLanguageToFront(this.detailLanguage_.language.code);

    this.closeMenuSoon_();
  },

  /**
   * Moves the language to the top of the list.
   * @private
   */
  onMoveToTopTap_: function() {
    /** @type {!CrActionMenuElement} */ (this.$.menu.get()).close();
    this.languageHelper.moveLanguageToFront(this.detailLanguage_.language.code);
  },

  /**
   * Moves the language up in the list.
   * @private
   */
  onMoveUpTap_: function() {
    /** @type {!CrActionMenuElement} */ (this.$.menu.get()).close();
    this.languageHelper.moveLanguage(
        this.detailLanguage_.language.code, true /* upDirection */);
  },

  /**
   * Moves the language down in the list.
   * @private
   */
  onMoveDownTap_: function() {
    /** @type {!CrActionMenuElement} */ (this.$.menu.get()).close();
    this.languageHelper.moveLanguage(
        this.detailLanguage_.language.code, false /* upDirection */);
  },

  /**
   * Disables the language.
   * @private
   */
  onRemoveLanguageTap_: function() {
    /** @type {!CrActionMenuElement} */ (this.$.menu.get()).close();
    this.languageHelper.disableLanguage(this.detailLanguage_.language.code);
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
  isProspectiveUILanguage_: function(languageCode, prospectiveUILanguage) {
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
  getLanguageItemClass_: function(languageCode, prospectiveUILanguage) {
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
  isCurrentInputMethod_: function(id, currentId) {
    return id == currentId;
  },

  /**
   * @param {string} id The input method ID.
   * @param {string} currentId The ID of the currently enabled input method.
   * @return {string} The class for the input method item.
   * @private
   */
  getInputMethodItemClass_: function(id, currentId) {
    return this.isCurrentInputMethod_(id, currentId) ? 'selected' : '';
  },

  /**
   * @param {!Event} e
   * @private
   */
  onDotsTap_: function(e) {
    // Set a copy of the LanguageState object since it is not data-bound to the
    // languages model directly.
    this.detailLanguage_ = /** @type {!LanguageState} */ (Object.assign(
        {},
        /** @type {!{model: !{item: !LanguageState}}} */ (e).model.item));

    // Ensure the template has been stamped.
    let menu = /** @type {?CrActionMenuElement} */ (this.$.menu.getIfExists());
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
  closeMenuSoon_: function() {
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
  onRestartTap_: function() {
    settings.LifetimeBrowserProxyImpl.getInstance().signOutAndRestart();
  },

  /**
   * Toggles the expand button within the element being listened to.
   * @param {!Event} e
   * @private
   */
  toggleExpandButton_: function(e) {
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
   * @return {string} The default tab index '0' if the selected input method is
   *     not currently enabled; otherwise, returns an empty string which
   *     effectively unsets the tabindex attribute.
   * @private
   */
  getInputMethodTabIndex_: function(id, currentId) {
    return id == currentId ? '' : '0';
  },

  /**
   * Handles the mousedown even by preventing focusing an input method list
   * item. This is only registered by the input method list item to avoid
   * unwanted focus.
   * @param {!Event} e
   * @private
   */
  onMouseDown_: function(e) {
    // Preventing the mousedown event from propagating prevents focus being set.
    e.preventDefault();
  },
});
})();
