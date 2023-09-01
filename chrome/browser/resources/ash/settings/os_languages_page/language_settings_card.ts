// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'language-settings-card' is the card element containing language settings.
 */

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../os_settings_page/settings_card.js';
import '../settings_shared.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isRevampWayfindingEnabled} from '../common/load_time_booleans.js';
import {PrefsState} from '../common/types.js';
import {RouteOriginMixin} from '../route_origin_mixin.js';
import {Router, routes} from '../router.js';

import {getTemplate} from './language_settings_card.html.js';
import {LanguageHelper, LanguagesModel} from './languages_types.js';

// The IME ID for the Accessibility Common extension used by Dictation.
const ACCESSIBILITY_COMMON_IME_ID =
    '_ext_ime_egfdjlfmgnehecnclamagfafdccgfndpdictation';

const LanguageSettingsCardElementBase =
    RouteOriginMixin(I18nMixin(PolymerElement));

export class LanguageSettingsCardElement extends
    LanguageSettingsCardElementBase {
  static get is() {
    return 'language-settings-card' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: {
        type: Object,
        notify: true,
      },

      languages: Object,

      languageHelper: Object,

      isRevampWayfindingEnabled_: Boolean,

      /**
       * This is enabled when any of the smart inputs features are allowed.
       */
      smartInputsEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('allowEmojiSuggestion');
        },
      },
    };
  }

  // Public API: Bidirectional data flow.
  /** Passed down to children. Do not access without using PrefsMixin. */
  prefs: PrefsState;

  // Public API: Downwards data flow.
  languages: LanguagesModel|undefined;
  languageHelper: LanguageHelper|undefined;

  // Internal state.
  private isRevampWayfindingEnabled_ = isRevampWayfindingEnabled();
  private smartInputsEnabled_: boolean;

  // Internal properties for mixins.
  // From RouteOriginMixin. This needs to be defined after
  // `isRevampWayfindingEnabled_`.
  override route = this.isRevampWayfindingEnabled_ ? routes.SYSTEM_PREFERENCES :
                                                     routes.OS_LANGUAGES;

  override ready(): void {
    super.ready();

    this.addFocusConfig(routes.OS_LANGUAGES_LANGUAGES, '#languagesRow');
    this.addFocusConfig(routes.OS_LANGUAGES_INPUT, '#inputRow');
    this.addFocusConfig(routes.OS_LANGUAGES_SMART_INPUTS, '#smartInputsRow');
  }

  private onLanguagesV2Click_(): void {
    Router.getInstance().navigateTo(routes.OS_LANGUAGES_LANGUAGES);
  }

  private onInputClick_(): void {
    Router.getInstance().navigateTo(routes.OS_LANGUAGES_INPUT);
  }

  private onSmartInputsClick_(): void {
    Router.getInstance().navigateTo(routes.OS_LANGUAGES_SMART_INPUTS);
  }

  /**
   * @param code The language code of the language.
   * @return The display name of the language specified.
   */
  private getLanguageDisplayName_(code: string|undefined): string {
    if (!code || !this.languageHelper) {
      return '';
    }
    const language = this.languageHelper.getLanguage(code);
    if (!language) {
      return '';
    }
    return language.displayName;
  }

  /**
   * @param id The input method ID.
   * @return The display name of the input method.
   */
  private getInputMethodDisplayName_(id: string|undefined): string {
    if (!id || !this.languageHelper) {
      return '';
    }
    if (id === ACCESSIBILITY_COMMON_IME_ID) {
      return '';
    }
    return this.languageHelper.getInputMethodDisplayName(id);
  }
}

customElements.define(
    LanguageSettingsCardElement.is, LanguageSettingsCardElement);

declare global {
  interface HTMLElementTagNameMap {
    [LanguageSettingsCardElement.is]: LanguageSettingsCardElement;
  }
}
