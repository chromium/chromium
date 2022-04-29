// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines a base class that represents a strategy for parsing
 * text and converting it into a macro.
 */

import {InputController} from '/accessibility_common/dictation/input_controller.js';
import {Macro} from '/accessibility_common/dictation/macros/macro.js';

/**
 * Represents a strategy for parsing speech input and converting it into a
 * Macro.
 */
export class ParseStrategy {
  /**
   * @param {!InputController} inputController
   * @param {boolean} isRTLLocale
   */
  constructor(inputController, isRTLLocale) {
    /** @private {!InputController} */
    this.inputController_ = inputController;
    /** @private {boolean} */
    this.isRTLLocale_ = isRTLLocale;
  }

  /** @return {!InputController} */
  getInputController() {
    return this.inputController_;
  }

  /** @return {boolean} */
  getIsRTLLocale() {
    return this.isRTLLocale_;
  }

  /**
   * Accepts text, parses it, and returns a Macro.
   * @param {string} text
   * @return {!Promise<?Macro>}
   */
  async parse(text) {}
}
