// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles user keyboard input events.
 */
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {Command} from './command.js';
import {KeyMap} from './key_map.js';
import {KeyUtil} from './key_util.js';

export namespace ChromeVoxKbHandler {

  /** The key map */
  export const handlerKeyMap = KeyMap.get();

  /**
   * Handler for ChromeVox commands. Returns undefined if the command does not
   * exist. Otherwise, returns the result of executing the command.
   */
  export let commandHandler: (command: Command) => (boolean | undefined);

  /**
   * Handles key down events.
   *
   * @param evt The key down event to process.
   * @return True if the default action should be performed.
   */
  export const basicKeyDownActionsListener = function(evt: KeyboardEvent):
      boolean {
        const keySequence = KeyUtil.keyEventToKeySequence(evt);
        const functionName =
            ChromeVoxKbHandler.handlerKeyMap.commandForKey(keySequence);

        // TODO (clchen): Disambiguate why functions are null. If the user
        // pressed something that is not a valid combination, make an error
        // noise so there is some feedback.

        if (!functionName) {
          return !KeyUtil.sequencing;
        }

        // This is the key event handler return value - true if the event should
        // propagate and the default action should be performed, false if we eat
        // the key.
        let returnValue = true;
        const commandResult = ChromeVoxKbHandler.commandHandler(functionName);
        if (commandResult !== undefined) {
          returnValue = commandResult;
        } else if (keySequence.cvoxModifier) {
          // Modifier/prefix is active -- prevent default action
          returnValue = false;
        }

        return returnValue;
      };
}

TestImportManager.exportForTesting(['ChromeVoxKbHandler', ChromeVoxKbHandler]);
