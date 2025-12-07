// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {RunMacroResult} from '/common/action_fulfillment/macros/macro.js';
import {Macro} from '/common/action_fulfillment/macros/macro.js';
import {MacroName} from '/common/action_fulfillment/macros/macro_names.js';

/**
 * Class that implements a macro to list Dictation commands (by opening a Help
 * Center article)
 */
export class ListCommandsMacro extends Macro {
  constructor() {
    super(MacroName.LIST_COMMANDS);
  }

  override run(): RunMacroResult {
    // Note that this will open a new tab, ending the current Dictation session
    // by changing the input focus.
    chrome.tabs.create(
        {
          url: 'https://support.google.com/chromebook?p=text_dictation_m100',
        },
        () => {});
    return this.createRunMacroResult_(/*isSuccess=*/ true);
  }
}
