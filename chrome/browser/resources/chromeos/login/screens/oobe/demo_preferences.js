// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/chromeos/cros_color_overrides.css.js';
import '//resources/polymer/v3_0/paper-styles/color.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/oobe_i18n_dropdown.js';
import '../../components/buttons/oobe_back_button.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';

import {assert} from '//resources/ash/common/assert.js';
import {I18nBehavior} from '//resources/ash/common/i18n_behavior.js';
import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {OobeDialogHostBehavior} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OobeTypes} from '../../components/oobe_types.js';
import {Oobe} from '../../cr_ui.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
const DemoPreferencesScreenBase = mixinBehaviors(
    [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],
    PolymerElement);

/**
 * @polymer
 */
class DemoPreferencesScreen extends DemoPreferencesScreenBase {
  static get is() {
    return 'demo-preferences-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

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

      /**
       * Indicates whether the next button is enabled and the user can continue.
       * @private {boolean}
       */
      user_can_continue_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
        computed: `userCanContinue_(retailer_name_input_,
                                    store_number_input_,
                                    is_country_selected_)`,
      },

      retailer_name_input_: {
        type: String,
        value: '',
      },

      store_number_input_: {
        type: String,
        value: '',
      },

      /**
       * Indicates whether the string entered for store_number_input_ is
       * invalid. Note that we have to use a negative boolean here so that we
       * can style the helper text based on this value.
       * @private {boolean}
       */
      store_number_input_invalid_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
        computed: 'isStoreNumberInputInvalid_(store_number_input_)',
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
    this.initializeLoginScreen('DemoPreferencesScreen');
    this.updateLocalizedContent();
  }

  /** Overridden from LoginScreenBehavior. */
  // clang-format off
  get EXTERNAL_API() {
    return [];
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
    if (this.configuration_applied_) {
      return;
    }
    const configuration = Oobe.getInstance().getOobeConfiguration();
    if (!configuration) {
      return;
    }
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

    const countryList = /** @type {!Array<OobeTypes.DemoCountryDsc>} */ (
        loadTimeData.getValue('demoModeCountryList'));
    this.setCountryList_(countryList);

    this.i18nUpdateLocale();
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
   * Sets country list.
   * @param {!Array<!OobeTypes.DemoCountryDsc>} countries
   * @private
   */
  setCountryList_(countries) {
    this.countries = countries;
    this.$.countryDropdownContainer.hidden = countries.length == 0;
    for (let i = 0; i < countries.length; ++i) {
      const country = countries[i];
      if (country.selected && country.value !== this.country_not_selected_id_) {
        this.is_country_selected_ = true;
        return;
      }
    }
  }

  /**
   * Determines whether the Next button is enabled and the user may continue.
   * Based on the country, retailer name, and store number preferences being
   * correctly set.
   *
   * @private
   */
  userCanContinue_(
      retailer_name_input_, store_number_input_, is_country_selected_) {
    return retailer_name_input_ &&
        RegExp('^[0-9]+$').test(store_number_input_) && is_country_selected_;
  }

  /**
   * Validates store number input for styling the input helper text. Note we
   * only consider the input invalid if it's nonempty, thus the different
   * pattern than in {@link userCanContinue_}
   *
   * @private
   */
  isStoreNumberInputInvalid_(store_number_input_) {
    return !RegExp('^[0-9]*$').test(store_number_input_);
  }

  /**
   * Handle country selection.
   * @param {!CustomEvent<!OobeTypes.DemoCountryDsc>} event
   * @private
   */
  onCountrySelected_(event) {
    this.userActed(['set-demo-mode-country', event.detail.value]);
    this.is_country_selected_ =
        event.detail.value !== this.country_not_selected_id_;
  }

  onInputKeyDown_(e) {
    if (e.key == 'Enter' &&
        this.userCanContinue_(
            this.retailer_name_input_, this.store_number_input_,
            this.is_country_selected_)) {
      this.onNextClicked_();
    }
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
    this.userActed([
      'continue-setup',
      this.retailer_name_input_,
      this.store_number_input_,
    ]);
  }
}

customElements.define(DemoPreferencesScreen.is, DemoPreferencesScreen);
