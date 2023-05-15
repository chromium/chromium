// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Macro} from './macro.js';
import {MacroName} from './macro_names.js';

/** Class that implements a macro to stop Dictation. */
export class StopListeningMacro extends Macro {
  constructor() {
    super(MacroName.STOP_LISTENING);
  }

  /** @override */
  run() {
    chrome.accessibilityPrivate.toggleDictation();
    return this.createRunMacroResult_(/*isSuccess=*/ true);
  }
}
