// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-languages-section' is the top-level settings section for
 * languages.
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './input_method_options_page.js';
import './input_page.js';
import './languages.js';
import './os_japanese_manage_user_dictionary_page.js';
import './os_languages_page_v2.js';
import './smart_inputs_page.js';
import '../os_settings_page/os_settings_animated_pages.js';
import '../os_settings_page/os_settings_subpage.js';
import '../../settings_shared.css.js';
import '../../settings_vars.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FocusConfig} from '../focus_config.js';
import {routes} from '../os_settings_routes.js';
import {Router} from '../router.js';

import {LanguageHelper, LanguagesModel} from './languages_types.js';
import {getTemplate} from './os_languages_section.html.js';

// The IME ID for the Accessibility Common extension used by Dictation.
const ACCESSIBILITY_COMMON_IME_ID =
    '_ext_ime_egfdjlfmgnehecnclamagfafdccgfndpdictation';

const OsSettingsLanguagesSectionElementBase = I18nMixin(PolymerElement);

class OsSettingsLanguagesSectionElement extends
    OsSettingsLanguagesSectionElementBase {
  static get is() {
    return 'os-settings-languages-section' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: Object,

      languages: {
        type: Object,
        notify: true,
      },

      languageHelper: Object,

      focusConfig_: {
        type: Object,
        value() {
          const map = new Map();
          if (routes.OS_LANGUAGES_SMART_INPUTS) {
            map.set(
                routes.OS_LANGUAGES_SMART_INPUTS.path,
                '#smartInputsSubpageTrigger');
          }
          return map;
        },
      },

      inputPageTitle_: {
        type: String,
        value(this: OsSettingsLanguagesSectionElement): string {
          const isUpdate2 =
              loadTimeData.getBoolean('enableLanguageSettingsV2Update2');
          return this.i18n(isUpdate2 ? 'inputPageTitleV2' : 'inputPageTitle');
        },
      },

      /**
       * This is enabled when any of the smart inputs features is allowed.
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
  prefs: unknown;

  // Internal state.
  private languages: LanguagesModel|undefined;
  // Only defined after a render.
  private languageHelper: LanguageHelper;
  private focusConfig_: FocusConfig;

  // loadTimeData flags and strings.
  private inputPageTitle_: string;
  private smartInputsEnabled_: boolean;

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
   * @param languageHelper The LanguageHelper object.
   * @return The display name of the language specified.
   */
  private getLanguageDisplayName_(
      code: string|undefined, languageHelper: LanguageHelper): string {
    if (!code) {
      return '';
    }
    const language = languageHelper.getLanguage(code);
    if (!language) {
      return '';
    }
    return language.displayName;
  }

  /**
   * @param id The input method ID.
   * @param languageHelper The LanguageHelper object.
   * @return The display name of the input method.
   */
  private getInputMethodDisplayName_(
      id: string|undefined, languageHelper: LanguageHelper): string {
    if (id === undefined) {
      return '';
    }

    if (id === ACCESSIBILITY_COMMON_IME_ID) {
      return '';
    }

    return languageHelper.getInputMethodDisplayName(id);
  }
}

customElements.define(
    OsSettingsLanguagesSectionElement.is, OsSettingsLanguagesSectionElement);

declare global {
  interface HTMLElementTagNameMap {
    [OsSettingsLanguagesSectionElement.is]: OsSettingsLanguagesSectionElement;
  }
}
