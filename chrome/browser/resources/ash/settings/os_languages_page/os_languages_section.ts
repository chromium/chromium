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
import '../os_settings_page/settings_card.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Section} from '../mojom-webui/routes.mojom-webui.js';
import {RouteOriginMixin} from '../route_origin_mixin.js';
import {Router, routes} from '../router.js';

import {LanguageHelper, LanguagesModel} from './languages_types.js';
import {getTemplate} from './os_languages_section.html.js';

// The IME ID for the Accessibility Common extension used by Dictation.
const ACCESSIBILITY_COMMON_IME_ID =
    '_ext_ime_egfdjlfmgnehecnclamagfafdccgfndpdictation';

const OsSettingsLanguagesSectionElementBase =
    RouteOriginMixin(I18nMixin(PolymerElement));

export class OsSettingsLanguagesSectionElement extends
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

      section_: {
        type: Number,
        value: Section.kLanguagesAndInput,
        readOnly: true,
      },

      languages: {
        type: Object,
        notify: true,
      },

      languageHelper: Object,

      inputPageTitle_: {
        type: String,
        value(this: OsSettingsLanguagesSectionElement): string {
          // TODO: b/263823772 - Inline this variable.
          const isUpdate2 = true;
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
  private section_: Section;

  // loadTimeData flags and strings.
  private inputPageTitle_: string;
  private smartInputsEnabled_: boolean;

  constructor() {
    super();

    /** RouteOriginMixin override */
    this.route = routes.OS_LANGUAGES;
  }

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
