// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles speech parsing for dictation.
 */

import {InputController} from '../input_controller.js';
import {Macro} from '../macros/macro.js';

import {InputTextStrategy} from './input_text_strategy.js';
import {ParseStrategy} from './parse_strategy.js';
import {PumpkinParseStrategy} from './pumpkin_parse_strategy.js';
import {SimpleParseStrategy} from './simple_parse_strategy.js';

/** SpeechParser handles parsing spoken transcripts into Macros. */
export class SpeechParser {
  /** @param {!InputController} inputController to interact with the IME. */
  constructor(inputController) {
    /** @private {boolean} */
    this.isRTLLocale_ = false;

    /** @private {!InputController} */
    this.inputController_ = inputController;

    /** @private {ParseStrategy} */
    this.inputTextStrategy_ = new InputTextStrategy(this.inputController_);

    /** @private {?ParseStrategy} */
    this.simpleParseStrategy_ = null;

    /** @private {?ParseStrategy} */
    this.pumpkinParseStrategy_ = null;
  }

  /**
   * @param {string} locale The Dictation recognition locale. Only some locales
   *     are supported by Pumpkin.
   * @return {!Promise}
   */
  async initialize(locale, commandsSupported) {
    this.isRTLLocale_ = SpeechParser.RTLLocales.has(locale);

    if (!commandsSupported) {
      this.simpleParseStrategy_ = null;
      this.pumpkinParseStrategy_ = null;
      return;
    }

    // Initialize additional parsing strategies.
    this.simpleParseStrategy_ =
        new SimpleParseStrategy(this.inputController_, this.isRTLLocale_);
    this.pumpkinParseStrategy_ = await PumpkinParseStrategy.create(
        this.inputController_, this.isRTLLocale_, locale);
  }

  /**
   * Parses user text to produce a macro command. Async to allow pumpkin to
   * complete loading if needed.
   * @param {string} text The text to parse.
   * @return {!Promise<!Macro>}
   */
  async parse(text) {
    // Try pumpkin parsing first.
    if (this.pumpkinParseStrategy_) {
      const macro = await this.pumpkinParseStrategy_.parse(text);
      if (macro) {
        return macro;
      }
    }

    // Fall-back to simple parsing.
    if (this.simpleParseStrategy_) {
      return await /** @type {!Promise<!Macro>} */ (
          this.simpleParseStrategy_.parse(text));
    }

    // Input text as-is as a catch-all.
    return await /** @type {!Promise<!Macro>} */ (
        this.inputTextStrategy_.parse(text));
  }

  /**
   * Determines whether commands are supported for this Dictation language
   * and UI system language.
   * @param {string} locale The Dictation locale code, like 'en-US'.
   * @param {string} systemLocale The system language, may be like 'en' or
   *     'en-US'.
   * @return boolean Whether commands are supported.
   */
  static areCommandsSupported(locale, systemLocale) {
    // Currently Dictation cannot support commands when the UI language
    // doesn't match the Dictation language. See crbug.com/1340590.
    locale = locale.toLowerCase();
    systemLocale = systemLocale.toLowerCase();
    const uiLanguage = systemLocale.split('-')[0];
    if (uiLanguage !== (locale.split('-')[0])) {
      if (SpeechParser.LocaleToUILanguagesMap.has(locale) &&
          (SpeechParser.LocaleToUILanguagesMap.get(locale) === uiLanguage ||
           SpeechParser.LocaleToUILanguagesMap.get(locale) === systemLocale)) {
        return true;
      }
      return false;
    }
    return true;
  }
}

// All RTL locales from Dictation::GetAllSupportedLocales.
SpeechParser.RTLLocales = new Set([
  'ar-AE', 'ar-BH', 'ar-DZ', 'ar-EG', 'ar-IL', 'ar-IQ', 'ar-JO',
  'ar-KW', 'ar-LB', 'ar-MA', 'ar-OM', 'ar-PS', 'ar-QA', 'ar-SA',
  'ar-TN', 'ar-YE', 'fa-IR', 'iw-IL', 'ur-IN', 'ur-PK',
]);

// Hebrew in Dictation is 'iw-IL' but 'he' in UI languages.
// yue-Hant-HK can map to 'zh-TW' because both are written as traditional
// Chinese. check this Norwegian in Dictation is 'no-NO' but 'nb' in UI
// languages.
SpeechParser.LocaleToUILanguagesMap =
    new Map([['iw-il', 'he'], ['yue-hant-hk', 'zh-tw'], ['no-no', 'nb']]);
