// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Abstract class for a keyboard handler.
 */
import {UserActionMonitor} from '../background/user_action_monitor.js';

import {Command} from './command_store.js';
import {KeyMap} from './key_map.js';
import {KeyUtil} from './key_util.js';

export class AbstractKeyboardHandler {
  /**
   * Handler for ChromeVox commands. Returns undefined if the command does not
   * exist. Otherwise, returns the result of executing the command.
   * @param {!Command} command
   * @return {boolean|undefined}
   */
  handleCommand(command) {}

  /**
   * Handles key down events.
   *
   * @param {Event} evt The key down event to process.
   * @return {boolean} True if the default action should be performed.
   */
  basicKeyDownActionsListener(evt) {
    const keySequence = KeyUtil.keyEventToKeySequence(evt);
    const functionName = KeyMap.get().commandForKey(keySequence);

    // TODO (clchen): Disambiguate why functions are null. If the user pressed
    // something that is not a valid combination, make an error noise so there
    // is some feedback.

    if (!functionName) {
      return !KeyUtil.sequencing;
    }

    // This is the key event handler return value - true if the event should
    // propagate and the default action should be performed, false if we eat
    // the key.
    let returnValue = true;
    const commandResult = this.handleCommand(functionName);
    if (commandResult !== undefined) {
      returnValue = commandResult;
    } else if (keySequence.cvoxModifier) {
      // Modifier/prefix is active -- prevent default action
      returnValue = false;
    }

    return returnValue;
  }
}
