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

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {Router} from '../router.js';
import {routes} from '../os_route.js';

import {LanguageHelper, LanguagesModel} from './languages_types.js';
import {getTemplate} from './os_languages_section.html.js';


// The IME ID for the Accessibility Common extension used by Dictation.
/** @type {string} */
const ACCESSIBILITY_COMMON_IME_ID =
    '_ext_ime_egfdjlfmgnehecnclamagfafdccgfndpdictation';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const OsSettingsLanguagesSectionElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class OsSettingsLanguagesSectionElement extends
    OsSettingsLanguagesSectionElementBase {
  static get is() {
    return 'os-settings-languages-section';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: Object,

      /** @type {!LanguagesModel|undefined} */
      languages: {
        type: Object,
        notify: true,
      },

      /** @type {!LanguageHelper} */
      languageHelper: Object,

      /** @private {!Map<string, string>} */
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

      /** @private */
      inputPageTitle_: {
        type: String,
        value() {
          const isUpdate2 =
              loadTimeData.getBoolean('enableLanguageSettingsV2Update2');
          return this.i18n(isUpdate2 ? 'inputPageTitleV2' : 'inputPageTitle');
        },
      },

      /**
       * This is enabled when any of the smart inputs features is allowed.
       * @private
       * */
      smartInputsEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('allowAssistivePersonalInfo') ||
              loadTimeData.getBoolean('allowEmojiSuggestion');
        },
      },

    };
  }

  /** @private */
  onLanguagesV2Click_() {
    Router.getInstance().navigateTo(routes.OS_LANGUAGES_LANGUAGES);
  }

  /** @private */
  onInputClick_() {
    Router.getInstance().navigateTo(routes.OS_LANGUAGES_INPUT);
  }

  /** @private */
  onSmartInputsClick_() {
    Router.getInstance().navigateTo(routes.OS_LANGUAGES_SMART_INPUTS);
  }

  /**
   * @param {string|undefined} code The language code of the language.
   * @param {!LanguageHelper} languageHelper The LanguageHelper object.
   * @return {string} The display name of the language specified.
   * @private
   */
  getLanguageDisplayName_(code, languageHelper) {
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
   * @param {string|undefined} id The input method ID.
   * @param {!LanguageHelper} languageHelper The LanguageHelper object.
   * @return {string} The display name of the input method.
   * @private
   */
  getInputMethodDisplayName_(id, languageHelper) {
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
