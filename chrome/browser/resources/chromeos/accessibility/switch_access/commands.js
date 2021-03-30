// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ActionManager} from './action_manager.js';
import {AutoScanManager} from './auto_scan_manager.js';
import {Navigator} from './navigator.js';

const SwitchAccessCommand = chrome.accessibilityPrivate.SwitchAccessCommand;

/**
 * Runs user commands.
 */
export class Commands {
  /** @private */
  constructor() {
    /**
     * A map from command name to the function binding for the command.
     * @private {!Map<!SwitchAccessCommand, !function(): void>}
     */
    this.commandMap_ = new Map([
      [SwitchAccessCommand.SELECT, ActionManager.onSelect],
      [
        SwitchAccessCommand.NEXT,
        Navigator.byItem.moveForward.bind(Navigator.byItem)
      ],
      [
        SwitchAccessCommand.PREVIOUS,
        Navigator.byItem.moveBackward.bind(Navigator.byItem)
      ]
    ]);

    chrome.accessibilityPrivate.onSwitchAccessCommand.addListener(
        this.runCommand_.bind(this));
  }

  static initialize() {
    Commands.instance = new Commands();
  }

  /**
   * Run the function binding for the specified command.
   * @param {!SwitchAccessCommand} command
   * @private
   */
  runCommand_(command) {
    this.commandMap_.get(command)();
    AutoScanManager.restartIfRunning();
  }
}
