// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines a base class that represents a strategy for parsing
 * text and converting it into a macro.
 */

import {InputController} from '/common/action_fulfillment/input_controller.js';
import {Macro} from '/common/action_fulfillment/macros/macro.js';

/**
 * Represents a strategy for parsing speech input and converting it into a
 * Macro.
 */
export class ParseStrategy {
  private inputController_: InputController;
  protected enabled = false;
  constructor(inputController: InputController) {
    this.inputController_ = inputController;
  }

  getInputController(): InputController {
    return this.inputController_;
  }

  isEnabled(): boolean {
    return this.enabled;
  }

  setEnabled(enabled: boolean): void {
    this.enabled = enabled;
  }

  /** Refreshes this strategy when the locale changes. */
  refresh(): void {}

  /** Accepts text, parses it, and returns a Macro. */
  async parse(text: string): Promise<Macro|null> {
    throw new Error(`The parse() function must be implemented by each subclass. Trying to parse text: ${text}`);
  }
}
