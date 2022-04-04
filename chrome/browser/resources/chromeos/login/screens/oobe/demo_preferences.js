// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* #js_imports_placeholder */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
const DemoPreferencesScreenBase = Polymer.mixinBehaviors(
    [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],
    Polymer.Element);

/**
 * @polymer
 */
class DemoPreferencesScreen extends DemoPreferencesScreenBase {
  static get is() {
    return 'demo-preferences-element';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {
      /**
       * List of languages for language selector dropdown.
       * @type {!Array<!OobeTypes.LanguageDsc>}
       */
      languages: {
        type: Array,
      },

      /**
       * List of keyboards for keyboard selector dropdown.
       * @type {!Array<!OobeTypes.IMEDsc>}
       */
      keyboards: {
        type: Array,
      },

      /**
       * List of countries for country selector dropdown.
       * @type {!Array<!OobeTypes.DemoCountryDsc>}
       */
      countries: {
        type: Array,
      },

      /**
       * Indicate whether a country has been selected.
       * @private {boolean}
       */
      is_country_selected_: {
        type: Boolean,
        value: false,
      },
    };
  }

  constructor() {
    super();

    /**
     * Flag that ensures that OOBE configuration is applied only once.
     * @private {boolean}
     */
    this.configuration_applied_ = false;

    /**
     * Country id of the option if no real country is selected.
     * @private {string}
     */
    this.country_not_selected_id_ = 'N/A';
  }

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('DemoPreferencesScreen', {
      resetAllowed: false,
    });
    this.updateLocalizedContent();
  }

  /** Overridden from LoginScreenBehavior. */
  // clang-format off
  get EXTERNAL_API() {
    return ['setSelectedKeyboard'];
  }
  // clang-format on

  /** Returns a control which should receive an initial focus. */
  get defaultControl() {
    return this.$.demoPreferencesDialog;
  }

  /** Called when dialog is shown */
  onBeforeShow() {
    window.setTimeout(this.applyOobeConfiguration_.bind(this), 0);
  }

  /** Called when dialog is shown for the first time */
  applyOobeConfiguration_() {
    if (this.configuration_applied_)
      return;
    const configuration = Oobe.getInstance().getOobeConfiguration();
    if (!configuration)
      return;
    if (configuration.demoPreferencesNext) {
      this.onNextClicked_();
    }
    this.configuration_applied_ = true;
  }

  /** Called after resources are updated. */
  updateLocalizedContent() {
    assert(loadTimeData);
    const languageList = /** @type {!Array<OobeTypes.LanguageDsc>} */ (
        loadTimeData.getValue('languageList'));
    this.setLanguageList_(languageList);

    const inputMethodsList = /** @type {!Array<OobeTypes.IMEDsc>} */ (
        loadTimeData.getValue('inputMethodsList'));
    this.setInputMethods_(inputMethodsList);

    const countryList = /** @type {!Array<OobeTypes.DemoCountryDsc>} */ (
        loadTimeData.getValue('demoModeCountryList'));
    this.setCountryList_(countryList);

    this.i18nUpdateLocale();
  }

  /**
   * Sets selected keyboard.
   * @param {string} keyboardId
   */
  setSelectedKeyboard(keyboardId) {
    let found = false;
    for (let keyboard of this.keyboards) {
      if (keyboard.value != keyboardId) {
        keyboard.selected = false;
        continue;
      }
      keyboard.selected = true;
      found = true;
    }
    if (!found)
      return;

    // Force i18n-dropdown to refresh.
    this.keyboards = this.keyboards.slice();
  }

  /**
   * Sets language list.
   * @param {!Array<!OobeTypes.LanguageDsc>} languages
   * @private
   */
  setLanguageList_(languages) {
    this.languages = languages;
  }

  /**
   * Sets input methods.
   * @param {!Array<!OobeTypes.IMEDsc>} inputMethods
   * @private
   */
  setInputMethods_(inputMethods) {
    this.keyboards = inputMethods;
  }

  /**
   * Sets country list.
   * @param {!Array<!OobeTypes.DemoCountryDsc>} countries
   * @private
   */
  setCountryList_(countries) {
    this.countries = countries;
    this.$.countryDropdownContainer.hidden = countries.length == 0;
    for (let i = 0; i < countries.length; ++i) {
      let country = countries[i];
      if (country.selected && country.value !== this.country_not_selected_id_) {
        this.is_country_selected_ = true;
        return;
      }
    }
  }

  /**
   * Handle country selection.
   * @param {!CustomEvent<!OobeTypes.DemoCountryDsc>} event
   * @private
   */
  onCountrySelected_(event) {
    chrome.send(
        'DemoPreferencesScreen.setDemoModeCountry', [event.detail.value]);
    this.is_country_selected_ =
        event.detail.value !== this.country_not_selected_id_;
  }

  /**
   * Back button click handler.
   * @private
   */
  onBackClicked_() {
    this.userActed('close-setup');
  }

  /**
   * Next button click handler.
   * @private
   */
  onNextClicked_() {
    this.userActed('continue-setup');
  }
}

customElements.define(DemoPreferencesScreen.is, DemoPreferencesScreen);
