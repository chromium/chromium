// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles speech parsing for dictation.
 */

import {InputController} from '../input_controller.js';
import {LocaleInfo} from '../locale_info.js';
import {Macro} from '../macros/macro.js';
import {MetricsUtils} from '../metrics_utils.js';

import {InputTextStrategy} from './input_text_strategy.js';
import {ParseStrategy} from './parse_strategy.js';
import {PumpkinParseStrategy} from './pumpkin_parse_strategy.js';
import {SimpleParseStrategy} from './simple_parse_strategy.js';

/** SpeechParser handles parsing spoken transcripts into Macros. */
export class SpeechParser {
  /** @param {!InputController} inputController to interact with the IME. */
  constructor(inputController) {
    /** @private {!InputController} */
    this.inputController_ = inputController;

    /** @private {!ParseStrategy} */
    this.inputTextStrategy_ = new InputTextStrategy(this.inputController_);

    /** @private {!ParseStrategy} */
    this.simpleParseStrategy_ = new SimpleParseStrategy(this.inputController_);

    /** @private {!ParseStrategy} */
    this.pumpkinParseStrategy_ =
        new PumpkinParseStrategy(this.inputController_);
  }

  /** Refreshes the speech parser when the locale changes. */
  refresh() {
    // Pumpkin has its own strings for command parsing, but we disable it when
    // commands aren't supported for consistency.
    this.simpleParseStrategy_.refresh();
    this.pumpkinParseStrategy_.refresh();
  }

  /**
   * Parses user text to produce a macro command.
   * @param {string} text The text to parse.
   * @return {!Promise<!Macro>}
   */
  async parse(text) {
    if (this.pumpkinParseStrategy_.isEnabled()) {
      MetricsUtils.recordPumpkinUsed(true);
      const macro = await this.pumpkinParseStrategy_.parse(text);
      if (macro) {
        return macro;
      }
    }

    // If we get here, then Pumpkin failed to parse `text`. There are cases
    // where this can happen e.g. if Pumpkin failed to initialize properly.
    // Try using `simpleParseStrategy_` as a fall-back.
    if (this.simpleParseStrategy_.isEnabled()) {
      MetricsUtils.recordPumpkinUsed(false);
      return await /** @type {!Promise<!Macro>} */ (
          this.simpleParseStrategy_.parse(text));
    }

    // Input text as-is as a catch-all.
    return await /** @type {!Promise<!Macro>} */ (
        this.inputTextStrategy_.parse(text));
  }

  /** For testing purposes only. */
  disablePumpkinForTesting() {
    this.pumpkinParseStrategy_.setEnabled(false);
  }
}
