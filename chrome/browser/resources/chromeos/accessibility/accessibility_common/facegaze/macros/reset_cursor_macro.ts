// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Macro, RunMacroResult} from '/common/action_fulfillment/macros/macro.js';
import {MacroName} from '/common/action_fulfillment/macros/macro_names.js';

import {MouseController} from '../mouse_controller.js';

/**
 * Class that implements a macro to reset the cursor position.
 */
export class ResetCursorMacro extends Macro {
  private mouseController_: MouseController;

  constructor(mouseController: MouseController) {
    super(MacroName.RESET_CURSOR);
    this.mouseController_ = mouseController;
  }

  override run(): RunMacroResult {
    this.mouseController_.resetLocation();
    return this.createRunMacroResult_(/*isSuccess=*/ true);
  }
}
