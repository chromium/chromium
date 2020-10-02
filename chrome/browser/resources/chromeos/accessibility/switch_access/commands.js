// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const SwitchAccessCommand = chrome.accessibilityPrivate.SwitchAccessCommand;

/**
 * Runs user commands.
 */
class Commands {
  /** @private */
  constructor() {
    /**
     * A map from command name to the function binding for the command.
     * @private {!Map<!SwitchAccessCommand, !function(): void>}
     */
    this.commandMap_ = new Map([
      [SwitchAccessCommand.SELECT, ActionManager.onSelect],
      [SwitchAccessCommand.NEXT, NavigationManager.moveForward],
      [SwitchAccessCommand.PREVIOUS, NavigationManager.moveBackward]
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
