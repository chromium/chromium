// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/paper-styles/color.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.m.js';
import '../../components/oobe_i18n_dropdown.js';
import '../../components/buttons/oobe_back_button.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/common_styles/oobe_common_styles.m.js';
import '../../components/common_styles/oobe_dialog_host_styles.m.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';

import {I18nBehavior} from '//resources/ash/common/i18n_behavior.js';
import {assert} from '//resources/ash/common/assert.js';
import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.m.js';
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

      is_input_invalid_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
        observer: 'isInputInvalidObserver_',
      },

      retailer_id_input_: {
        type: String,
        value: '',
        observer: 'retailerIdObserver_',
      },

      retailer_id_input_pattern_: {
        type: String,
        value: '^[A-Z]{3}-[0-9]{4}$',
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

  getRetailerIdInputDisplayText_() {
    if (this.is_input_invalid_) {
      return this.i18n('retailerIdInputErrorText');
    }
    return this.i18n('retailerIdInputHelpText');
  }

  retailerIdObserver_() {
    if (!this.retailer_id_input_) {
      this.is_input_invalid_ = false;
    } else {
      this.is_input_invalid_ = !RegExp(this.retailer_id_input_pattern_)
                                    .test(this.retailer_id_input_);
    }
  }

  isInputInvalidObserver_() {
    if (this.is_input_invalid_) {
      this.$.nextButton.disabled = true;
    } else {
      this.$.nextButton.disabled = false;
    }
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

  onKeydownRetailerIdInput_(e) {
    if (e.key == 'Enter') {
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
    this.userActed(['continue-setup', this.retailer_id_input_]);
  }
}

customElements.define(DemoPreferencesScreen.is, DemoPreferencesScreen);
