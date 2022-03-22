// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The IME ID for the Accessibility Common extension used by Dictation.
/** @type {string} */
const ACCESSIBILITY_COMMON_IME_ID =
    '_ext_ime_egfdjlfmgnehecnclamagfafdccgfndpdictation';

/**
 * @fileoverview
 * 'os-settings-languages-section' is the top-level settings section for
 * languages.
 */
import {afterNextRender, Polymer, html, flush, Templatizer, TemplateInstanceBase} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import '//resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import '//resources/cr_elements/shared_vars_css.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import './input_page.js';
import './os_languages_page_v2.js';
import './smart_inputs_page.js';
import './input_method_options_page.js';
import {loadTimeData} from '../../i18n_setup.js';
import './languages.js';
import {routes} from '../os_route.js';
import {Router, Route} from '../../router.js';
import '../../settings_page/settings_animated_pages.js';
import '../../settings_page/settings_subpage.js';
import '../../settings_shared_css.js';
import '../../settings_vars_css.js';
import {LanguageHelper, LanguagesModel} from './languages_types.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'os-settings-languages-section',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
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
    }
  },

  /** @private */
  onLanguagesV2Click_() {
    Router.getInstance().navigateTo(routes.OS_LANGUAGES_LANGUAGES);
  },

  /** @private */
  onInputClick_() {
    Router.getInstance().navigateTo(routes.OS_LANGUAGES_INPUT);
  },

  /** @private */
  onSmartInputsClick_() {
    Router.getInstance().navigateTo(routes.OS_LANGUAGES_SMART_INPUTS);
  },

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
  },

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
  },
});
