// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CheckContextResult, RunMacroResult} from '/common/action_fulfillment/macros/macro.js';
import {Macro, MacroError, ToggleDirection} from '/common/action_fulfillment/macros/macro.js';
import {MacroName} from '/common/action_fulfillment/macros/macro_names.js';

import type {MouseController} from '../mouse_controller.js';

/** Class that implements a macro to toggle a long click action. */
export class MouseLongClickMacro extends Macro {
  private mouseController_: MouseController;

  constructor(mouseController: MouseController) {
    super(MacroName.MOUSE_LONG_CLICK_LEFT);
    this.mouseController_ = mouseController;
  }

  override isToggle(): boolean {
    return true;
  }

  override getToggleDirection(): ToggleDirection {
    return this.mouseController_.isLongClickActive() ? ToggleDirection.OFF :
                                                       ToggleDirection.ON;
  }

  override checkContext(): CheckContextResult {
    if (!this.mouseController_.mouseLocation()) {
      return this.createFailureCheckContextResult_(MacroError.BAD_CONTEXT);
    }
    return this.createSuccessCheckContextResult_();
  }

  override run(): RunMacroResult {
    if (!this.mouseController_.mouseLocation()) {
      return this.createRunMacroResult_(/*isSuccess=*/ false);
    }

    this.mouseController_.toggleLongClick();
    return this.createRunMacroResult_(/*isSuccess=*/ true);
  }
}
