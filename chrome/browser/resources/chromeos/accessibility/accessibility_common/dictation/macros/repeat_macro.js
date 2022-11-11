// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Macro} from './macro.js';
import {MacroName} from './macro_names.js';

/** Implements a macro that repeats the last executed macro. */
export class RepeatMacro extends Macro {
  constructor() {
    super(MacroName.REPEAT);
  }
}
