// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-manage-input-methods-page' is a sub-page for enabling
 * and disabling input methods. Input methods are grouped by base languages to
 * avoid showing duplicate or ambiguous input methods.
 *
 * TODO(hsuregan): Move to OS settings.
 */
Polymer({
  is: 'settings-manage-input-methods-page',

  properties: {
    /** @type {!LanguagesModel|undefined} */
    languages: {
      type: Object,
      notify: true,
    },

    /** @type {!LanguageHelper} */
    languageHelper: Object,

    /**
     * List of enabled languages with the input methods to show.
     * @private {!Array<
     *     !{language: !chrome.languageSettingsPrivate.Language,
     *       inputMethods: !Array<!chrome.languageSettingsPrivate.InputMethod>
     *      }>}
     */
    languageList_: {
      type: Array,
      value: function() {
        return [];
      },
    },
  },

  observers: [
    'availableInputMethodsChanged_(languages.enabled.*,' +
        'languages.inputMethods.supported.*)',
    'enabledInputMethodsChanged_(languages.inputMethods.enabled.*)',
  ],

  /** @private */
  availableInputMethodsChanged_: function() {
    this.populateLanguageList_();
  },

  /** @private */
  enabledInputMethodsChanged_: function() {
    this.populateLanguageList_();
  },

  /**
   * Handler for an input method checkbox.
   * @param {!{model: !{item: chrome.languageSettingsPrivate.InputMethod},
   *           target: !Element}} e
   * @private
   */
  onCheckboxChange_: function(e) {
    // TODO(michaelpg): Show confirmation dialog for 3rd-party IMEs.
    const id = e.model.item.id;
    if (e.target.checked) {
      this.languageHelper.addInputMethod(id);
    } else {
      this.languageHelper.removeInputMethod(id);
    }
  },

  /**
   * Returns true if the input method can be added/removed.
   * @param {!chrome.languageSettingsPrivate.InputMethod} targetInputMethod
   * @param {!Object} change Polymer change object (provided in the HTML so this
   *     gets called whenever languages.inputMethods.enabled.* changes).
   * @return {boolean}
   * @private
   */
  enableInputMethodCheckbox_: function(targetInputMethod, change) {
    if (targetInputMethod.isProhibitedByPolicy) {
      return false;
    }

    if (!targetInputMethod.enabled) {
      return true;
    }

    // Third-party IMEs can always be removed.
    if (!this.languageHelper.isComponentIme(targetInputMethod)) {
      return true;
    }

    // Can be removed as long as there is another component IME.
    return this.languages.inputMethods.enabled.some(function(inputMethod) {
      return inputMethod != targetInputMethod &&
          this.languageHelper.isComponentIme(inputMethod);
    }, this);
  },

  /**
   * Creates the list of languages and their input methods as the data source
   * for the view.
   * @private
   */
  populateLanguageList_: function() {
    const languageList = [];

    // Languages that have already been listed further up.
    const /** !Set<string> */ usedLanguages = new Set();

    // Add languages in preference order. However, if there are multiple
    // enabled variants of the same base language, group them all as the base
    // language instead of showing each variant individually. This prevents us
    // from displaying duplicate input methods under different variants.
    for (let i = 0; i < this.languages.enabled.length; i++) {
      const languageState = this.languages.enabled[i];
      // Skip the language if we have already included it or its base language.
      if (usedLanguages.has(languageState.language.code)) {
        continue;
      }
      const baseLanguageCode = this.languageHelper.getLanguageCodeWithoutRegion(
          languageState.language.code);
      if (usedLanguages.has(baseLanguageCode)) {
        continue;
      }

      // Find the other languages further down in the preferred languages list
      // which also use this language's base language code.
      const languageFamilyCodes = [languageState.language.code];
      for (let j = i + 1; j < this.languages.enabled.length; j++) {
        const otherCode = this.languages.enabled[j].language.code;
        if (this.languageHelper.getLanguageCodeWithoutRegion(otherCode) ==
            baseLanguageCode) {
          languageFamilyCodes.push(this.languages.enabled[j].language.code);
        }
      }

      const combinedInputMethods =
          this.getInputMethodsForLanguages(languageFamilyCodes);

      // Skip the language if it has no new input methods.
      if (!combinedInputMethods.length) {
        continue;
      }

      // Add the language or base language.
      let displayLanguage = languageState.language;
      if (languageFamilyCodes.length > 1) {
        const baseLanguage = this.languageHelper.getLanguage(baseLanguageCode);
        if (baseLanguage) {
          displayLanguage = baseLanguage;
        }
      }
      languageList.push({
        language: displayLanguage,
        inputMethods: combinedInputMethods,
      });
      for (let k = 0; k < languageFamilyCodes.length; k++) {
        usedLanguages.add(languageFamilyCodes[k]);
      }
    }

    // Add ARC IMEs to the bottom if any.
    const arcInputMethods = this.getInputMethodsForLanguages(
        [this.languageHelper.getArcImeLanguageCode()]);
    if (arcInputMethods.length) {
      languageList.push({
        language: this.languageHelper.getLanguage(
            this.languageHelper.getArcImeLanguageCode()),
        inputMethods: arcInputMethods,
      });
    }

    this.languageList_ = languageList;
    this.notifyInputMethodsChanged_();
  },

  /**
   * Returns the input methods that support any of the given languages.
   * @param {!Array<string>} languageCodes
   * @return {!Array<!chrome.languageSettingsPrivate.InputMethod>}
   * @private
   */
  getInputMethodsForLanguages: function(languageCodes) {
    // Input methods that have already been listed for this language.
    const /** !Set<string> */ usedInputMethods = new Set();
    /** @type {!Array<chrome.languageSettingsPrivate.InputMethod>} */
    const combinedInputMethods = [];
    for (let i = 0; i < languageCodes.length; i++) {
      const inputMethods =
          this.languageHelper.getInputMethodsForLanguage(languageCodes[i]);
      // Get the language's unused input methods and mark them as used.
      const newInputMethods = inputMethods.filter(function(inputMethod) {
        if (usedInputMethods.has(inputMethod.id)) {
          return false;
        }
        usedInputMethods.add(inputMethod.id);
        return true;
      });
      [].push.apply(combinedInputMethods, newInputMethods);
    }
    return combinedInputMethods;
  },

  // TODO(Polymer/polymer#3603): We have to notify Polymer of properties that
  // may have changed on nested objects, even when the outer property itself
  // is set to a new array.
  // TODO(michaelpg): Test this behavior.
  /** @private */
  notifyInputMethodsChanged_: function() {
    for (let i = 0; i < this.languageList_.length; i++) {
      for (let j = 0; j < this.languageList_[i].inputMethods.length; j++) {
        this.notifyPath(
            'languageList_.' + i + '.inputMethods.' + j + '.enabled',
            this.languageList_[i].inputMethods[j].enabled);
      }
    }
  },

  /**
   * @param {Object} allowedInputMethods
   * @return {boolean}
   * @private
   */
  inputMethodsLimitedByPolicy_: function(allowedInputMethods) {
    return !!allowedInputMethods && allowedInputMethods.value.length > 0;
  }
});
