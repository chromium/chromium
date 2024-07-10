// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/oobe_i18n_dropdown.js';
import '../../components/buttons/oobe_back_button.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';

import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';
import type {OobeTypes} from '../../components/oobe_types.js';
import {Oobe} from '../../cr_ui.js';

import {getTemplate} from './demo_preferences.html.js';

// The retailer name input has the max length of 256 characters.
const RETAILER_NAME_INPUT_MAX_LENGTH = 256;
// The store number input has the max length of 256 characters.
const STORE_NUMBER_INPUT_MAX_LENGTH = 256;

const DemoPreferencesScreenBase =
    OobeDialogHostMixin(LoginScreenMixin(OobeI18nMixin(PolymerElement)));

export class DemoPreferencesScreen extends DemoPreferencesScreenBase {
  static get is() {
    return 'demo-preferences-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * List of languages for language selector dropdown.
       */
      languages: {
        type: Array,
      },

      /**
       * List of countries for country selector dropdown.
       */
      countries: {
        type: Array,
      },

      /**
       * Indicate whether a country has been selected.
       */
      isCountrySelected: {
        type: Boolean,
        value: false,
      },

      /**
       * Indicates whether the next button is enabled and the user can continue.
       */
      userCanContinue: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
        computed: `userCanContinue_(retailerNameInput,
                                    storeNumberInput,
                                    isCountrySelected)`,
      },

      retailerNameInput: {
        type: String,
        value: '',
      },

      storeNumberInput: {
        type: String,
        value: '',
      },

      /**
       * Indicates whether the string entered for storeNumberInput is
       * invalid. Note that we have to use a negative boolean here so that we
       * can style the helper text based on this value.
       */
      storeNumberInputInvalid: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
        computed: 'isStoreNumberInputInvalid_(storeNumberInput)',
      },
    };
  }

  languages: OobeTypes.LanguageDsc[];
  countries: OobeTypes.DemoCountryDsc[];
  private isCountrySelected: boolean;
  private userCanContinue: boolean;
  retailerNameInput: string;
  storeNumberInput: string;
  private storeNumberInputInvalid: boolean;
  private configurationApplied: boolean;
  private countryNotSelectedId: string;

  constructor() {
    super();

    /**
     * Flag that ensures that OOBE configuration is applied only once.
     */
    this.configurationApplied = false;

    /**
     * Country id of the option if no real country is selected.
     */
    this.countryNotSelectedId = 'N/A';
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('DemoPreferencesScreen');
    this.updateLocalizedContent();
  }

  /** Returns a control which should receive an initial focus. */
  override get defaultControl(): HTMLElement {
    return this.shadowRoot!.getElementById('demoPreferencesDialog')!;
  }

  /** Called when dialog is shown */
  override onBeforeShow(): void {
    super.onBeforeShow();
    window.setTimeout(this.applyOobeConfiguration_.bind(this), 0);
  }

  /** Called when dialog is shown for the first time */
  private applyOobeConfiguration_(): void {
    if (this.configurationApplied) {
      return;
    }
    const configuration = Oobe.getInstance().getOobeConfiguration();
    if (!configuration) {
      return;
    }
    if (configuration.demoPreferencesNext) {
      this.onNextClicked_();
    }
    this.configurationApplied = true;
  }

  /** Called after resources are updated. */
  override updateLocalizedContent(): void {
    assert(loadTimeData);
    const languageList: OobeTypes.LanguageDsc[] =
        loadTimeData.getValue('languageList');
    this.setLanguageList_(languageList);

    const countryList: OobeTypes.DemoCountryDsc[] =
        loadTimeData.getValue('demoModeCountryList');
    this.setCountryList_(countryList);

    this.i18nUpdateLocale();
  }

  /**
   * Sets language list.
   */
  private setLanguageList_(languages: OobeTypes.LanguageDsc[]): void {
    this.languages = languages;
  }

  /**
   * Sets country list.
   */
  private setCountryList_(countries: OobeTypes.DemoCountryDsc[]): void {
    this.countries = countries;
    this.shadowRoot!.getElementById('countryDropdownContainer')!.hidden =
        countries.length === 0;
    for (let i = 0; i < countries.length; ++i) {
      const country = countries[i];
      if (country.selected && country.value !== this.countryNotSelectedId) {
        this.isCountrySelected = true;
        return;
      }
    }
  }

  /**
   * Determines whether the Next button is enabled and the user may continue.
   * Based on the country, retailer name, and store number preferences being
   * correctly set.
   *
   * We need to check all fields (parameters) are not undefined, not null and
   * non-empty (console.log(!!"") => false) before checking their value or
   * length.
   *
   * The retailer name must be a non-empty string in the max length of 256
   * characters.
   * The store number must be a non-empty numerical string in the max length
   * of 256 characters.
   *
   * TODO(b/324086625): Add help text of the string length limit on the
   * retailer name field and store number field.
   */
  private userCanContinue_(
      retailerNameInput: string, storeNumberInput: string,
      isCountrySelected: boolean): boolean {
    return !!retailerNameInput && !!isCountrySelected && !!storeNumberInput &&
        isCountrySelected &&
        storeNumberInput.length <= STORE_NUMBER_INPUT_MAX_LENGTH &&
        retailerNameInput.length <= RETAILER_NAME_INPUT_MAX_LENGTH &&
        RegExp('^[0-9]+$').test(storeNumberInput);
  }

  /**
   * Validates store number input for styling the input helper text. Note we
   * only consider the input invalid if it's nonempty, thus the different
   * pattern than in {@link userCanContinue_}
   *
   * TODO(b/324086625): Add help text of the string length limit on the
   * retailer name field and store number field on the Demo Mode preferences
   * screen.
   */
  private isStoreNumberInputInvalid_(storeNumberInput: string): boolean {
    return !RegExp('^[0-9]*$').test(storeNumberInput);
  }

  /**
   * Handle country selection.
   */
  private onCountrySelected_(event: CustomEvent<OobeTypes.DemoCountryDsc>):
      void {
    this.userActed(['set-demo-mode-country', event.detail.value]);
    this.isCountrySelected = event.detail.value !== this.countryNotSelectedId;
  }

  private onInputKeyDown_(e: KeyboardEvent): void {
    if (e.key === 'Enter' &&
        this.userCanContinue_(
            this.retailerNameInput, this.storeNumberInput,
            this.isCountrySelected)) {
      this.onNextClicked_();
    }
  }

  /**
   * Back button click handler.
   */
  private onBackClicked_(): void {
    this.userActed('close-setup');
  }

  /**
   * Next button click handler.
   */
  private onNextClicked_(): void {
    this.userActed([
      'continue-setup',
      this.retailerNameInput,
      this.storeNumberInput,
    ]);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [DemoPreferencesScreen.is]: DemoPreferencesScreen;
  }
}

customElements.define(DemoPreferencesScreen.is, DemoPreferencesScreen);
