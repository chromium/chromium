// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Macro, RunMacroResult, ToggleDirection} from './macro.js';
import {MacroName} from './macro_names.js';

/** Class that implements a macro that runs the supplied callback. */
export class CustomCallbackMacro extends Macro {
  private callback_: () => void;
  private toggleDirection_: ToggleDirection = ToggleDirection.NONE;

  constructor(
      macroName: MacroName, callback: () => void,
      toggleDirection?: ToggleDirection) {
    super(macroName);
    this.callback_ = callback;

    if (toggleDirection) {
      this.toggleDirection_ = toggleDirection;
    }
  }

  override isToggle(): boolean {
    return this.toggleDirection_ !== ToggleDirection.NONE;
  }

  override getToggleDirection(): ToggleDirection {
    return this.toggleDirection_;
  }

  override run(): RunMacroResult {
    this.callback_();
    return this.createRunMacroResult_(/*isSuccess=*/ true);
  }
}
