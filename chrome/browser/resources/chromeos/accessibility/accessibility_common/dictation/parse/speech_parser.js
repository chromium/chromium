// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles speech parsing for dictation.
 */

import {InputController} from '/accessibility_common/dictation/input_controller.js';
import {Macro} from '/accessibility_common/dictation/macros/macro.js';
import {InputTextStrategy} from '/accessibility_common/dictation/parse/input_text_strategy.js';
import {ParseStrategy} from '/accessibility_common/dictation/parse/parse_strategy.js';
import {PumpkinParseStrategy} from '/accessibility_common/dictation/parse/pumpkin_parse_strategy.js';
import {SimpleParseStrategy} from '/accessibility_common/dictation/parse/simple_parse_strategy.js';

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
   */
  async initialize(locale) {
    this.isRTLLocale_ = SpeechParser.RTLLocales.has(locale);

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
}

// All RTL locales from Dictation::GetAllSupportedLocales.
SpeechParser.RTLLocales = new Set([
  'ar-AE', 'ar-BH', 'ar-DZ', 'ar-EG', 'ar-IL', 'ar-IQ', 'ar-JO',
  'ar-KW', 'ar-LB', 'ar-MA', 'ar-OM', 'ar-PS', 'ar-QA', 'ar-SA',
  'ar-TN', 'ar-YE', 'fa-IR', 'iw-IL', 'ur-IN', 'ur-PK'
]);
