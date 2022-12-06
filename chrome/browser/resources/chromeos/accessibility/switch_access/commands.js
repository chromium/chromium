// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ActionManager} from './action_manager.js';
import {AutoScanManager} from './auto_scan_manager.js';
import {Navigator} from './navigator.js';

const Command = chrome.accessibilityPrivate.SwitchAccessCommand;

/** Runs user commands. */
export class SACommands {
  constructor() {
    /**
     * A map from command name to the function binding for the command.
     * @private {!Map<!Command, !function(): void>}
     */
    this.commandMap_ = new Map([
      [Command.SELECT, ActionManager.onSelect],
      [Command.NEXT, () => Navigator.byItem.moveForward()],
      [Command.PREVIOUS, () => Navigator.byItem.moveBackward()],
    ]);

    chrome.accessibilityPrivate.onSwitchAccessCommand.addListener(
        command => this.runCommand_(command));
  }

  /**
   * Run the function binding for the specified command.
   * @param {!Command} command
   * @private
   */
  runCommand_(command) {
    this.commandMap_.get(command)();
    AutoScanManager.restartIfRunning();
  }
}
