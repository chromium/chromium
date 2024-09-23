// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Macro, RunMacroResult} from './macro.js';
import {MacroName} from './macro_names.js';

/** Class that implements a macro that runs the supplied callback. */
export class CustomCallbackMacro extends Macro {
  private callback_: () => void;

  constructor(macroName: MacroName, callback: () => void) {
    super(macroName);
    this.callback_ = callback;
  }

  override run(): RunMacroResult {
    this.callback_();
    return this.createRunMacroResult_(/*isSuccess=*/ true);
  }
}
