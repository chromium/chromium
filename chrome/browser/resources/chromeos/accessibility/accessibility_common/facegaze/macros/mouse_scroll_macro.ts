// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Macro, RunMacroResult} from '/common/action_fulfillment/macros/macro.js';
import {MacroName} from '/common/action_fulfillment/macros/macro_names.js';

import {MouseController} from '../mouse_controller.js';

/** Class that implements a macro to toggle scroll mode. */
export class MouseScrollMacro extends Macro {
  private mouseController_: MouseController;

  constructor(mouseController: MouseController) {
    super(MacroName.TOGGLE_SCROLL_MODE);
    this.mouseController_ = mouseController;
  }

  override run(): RunMacroResult {
    this.mouseController_.toggleScrollMode();
    return this.createRunMacroResult_(/*isSuccess=*/ true);
  }
}
