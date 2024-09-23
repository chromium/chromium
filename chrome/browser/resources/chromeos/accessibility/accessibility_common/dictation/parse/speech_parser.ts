// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles speech parsing for dictation.
 */
import {InputController} from '/common/action_fulfillment/input_controller.js';
import {Macro} from '/common/action_fulfillment/macros/macro.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {MetricsUtils} from '../metrics_utils.js';

import {InputTextStrategy} from './input_text_strategy.js';
import {ParseStrategy} from './parse_strategy.js';
import {PumpkinParseStrategy} from './pumpkin_parse_strategy.js';
import {SimpleParseStrategy} from './simple_parse_strategy.js';

/** SpeechParser handles parsing spoken transcripts into Macros. */
export class SpeechParser {
  private inputController_: InputController;
  private inputTextStrategy_: ParseStrategy;
  private simpleParseStrategy_: ParseStrategy;
  private pumpkinParseStrategy_: ParseStrategy;

  /** @param inputController to interact with the IME. */
  constructor(inputController: InputController) {
    this.inputController_ = inputController;
    this.inputTextStrategy_ = new InputTextStrategy(this.inputController_);
    this.simpleParseStrategy_ = new SimpleParseStrategy(this.inputController_);
    this.pumpkinParseStrategy_ =
        new PumpkinParseStrategy(this.inputController_);
  }

  /** Refreshes the speech parser when the locale changes. */
  refresh(): void {
    // Pumpkin has its own strings for command parsing, but we disable it when
    // commands aren't supported for consistency.
    this.simpleParseStrategy_.refresh();
    this.pumpkinParseStrategy_.refresh();
  }

  /**
   * Parses user text to produce a macro command.
   * @param text The text to parse.
   */
  async parse(text: string): Promise<Macro> {
    if (this.pumpkinParseStrategy_.isEnabled()) {
      MetricsUtils.recordPumpkinUsed(true);
      const macro = await this.pumpkinParseStrategy_.parse(text);
      MetricsUtils.recordPumpkinSucceeded(Boolean(macro));
      if (macro) {
        return macro;
      }
    }

    // If we get here, then Pumpkin failed to parse `text`. There are cases
    // where this can happen e.g. if Pumpkin failed to initialize properly.
    // Try using `simpleParseStrategy_` as a fall-back.
    if (this.simpleParseStrategy_.isEnabled()) {
      MetricsUtils.recordPumpkinUsed(false);
      return await this.simpleParseStrategy_.parse(text) as Macro;
    }

    // Input text as-is as a catch-all.
    MetricsUtils.recordPumpkinUsed(false);
    return await this.inputTextStrategy_.parse(text) as Macro;
  }

  /** For testing purposes only. */
  disablePumpkinForTesting(): void {
    this.pumpkinParseStrategy_.setEnabled(false);
  }
}

TestImportManager.exportForTesting(SpeechParser);
