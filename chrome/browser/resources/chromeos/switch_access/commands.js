// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const SwitchAccessCommand = chrome.accessibilityPrivate.SwitchAccessCommand;
/**
 * Class to run and get details about user commands.
 */
class Commands {
  /**
   * @param {SwitchAccessInterface} switchAccess
   */
  constructor(switchAccess) {
    /**
     * SwitchAccess reference.
     * @private {SwitchAccessInterface}
     */
    this.switchAccess_ = switchAccess;

    /**
     * A map from command name to the function binding for the command.
     * @private {!Map<!SwitchAccessCommand, !function(): void>}
     */
    this.commandMap_ = this.buildCommandMap_();

    this.init_();
  }

  /**
   * Starts listening for Switch Access command events.
   * @private
   */
  init_() {
    chrome.accessibilityPrivate.onSwitchAccessCommand.addListener(
        this.runCommand_.bind(this));
  }

  /**
   * Run the function binding for the specified command.
   * @param {!SwitchAccessCommand} command
   * @private
   */
  runCommand_(command) {
    this.commandMap_.get(command)();
    this.switchAccess_.restartAutoScan();
  }

  /**
   * Build a map from command name to the function binding for the command.
   * @return {!Map<!SwitchAccessCommand, !function(): void>}
   * @private
   */
  buildCommandMap_() {
    return new Map([
      [
        SwitchAccessCommand.SELECT,
        this.switchAccess_.enterMenu.bind(this.switchAccess_)
      ],
      [
        SwitchAccessCommand.NEXT,
        this.switchAccess_.moveForward.bind(this.switchAccess_)
      ],
      [
        SwitchAccessCommand.PREVIOUS,
        this.switchAccess_.moveBackward.bind(this.switchAccess_)
      ]
    ]);
  }
}
