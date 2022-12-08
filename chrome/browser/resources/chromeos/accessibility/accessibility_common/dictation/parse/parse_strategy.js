// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines a base class that represents a strategy for parsing
 * text and converting it into a macro.
 */

import {InputController} from '../input_controller.js';
import {Macro} from '../macros/macro.js';

/**
 * Represents a strategy for parsing speech input and converting it into a
 * Macro.
 */
export class ParseStrategy {
  /** @param {!InputController} inputController */
  constructor(inputController) {
    /** @private {!InputController} */
    this.inputController_ = inputController;
    /** @protected {boolean} */
    this.enabled = false;
  }

  /** @return {!InputController} */
  getInputController() {
    return this.inputController_;
  }

  /** @return {boolean} */
  isEnabled() {
    return this.enabled;
  }

  /** @param {boolean} enabled */
  setEnabled(enabled) {
    this.enabled = enabled;
  }

  /** Refreshes this strategy when the locale changes. */
  refresh() {}

  /**
   * Accepts text, parses it, and returns a Macro.
   * @param {string} text
   * @return {!Promise<?Macro>}
   */
  async parse(text) {}
}
