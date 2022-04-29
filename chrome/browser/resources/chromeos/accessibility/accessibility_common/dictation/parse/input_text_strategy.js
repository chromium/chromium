// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {InputController} from '/accessibility_common/dictation/input_controller.js';
import {InputTextViewMacro} from '/accessibility_common/dictation/macros/input_text_view_macro.js';
import {ParseStrategy} from '/accessibility_common/dictation/parse/parse_strategy.js';

/** A parsing strategy that tells text to be input as-is. */
export class InputTextStrategy extends ParseStrategy {
  /** @param {!InputController} inputController */
  constructor(inputController) {
    super(inputController, false);
  }

  /** @override */
  async parse(text) {
    return new InputTextViewMacro(text, this.getInputController());
  }
}
