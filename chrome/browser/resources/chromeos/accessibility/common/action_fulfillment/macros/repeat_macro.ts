// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Context} from '../context_checker.js';

import {CheckContextResult, Macro, MacroError} from './macro.js';
import {MacroName} from './macro_names.js';

/** Implements a macro that repeats the last executed macro. */
export class RepeatMacro extends Macro {
  constructor() {
    super(MacroName.REPEAT);
  }

  /**
   * This always returns a failure because RepeatMacro is never actually run,
   * it's just a placeholder that is swapped out for the previously executed
   * macro.
   */
  override checkContext(): CheckContextResult {
    return this.createFailureCheckContextResult_(
        MacroError.BAD_CONTEXT, Context.NO_PREVIOUS_MACRO);
  }
}
