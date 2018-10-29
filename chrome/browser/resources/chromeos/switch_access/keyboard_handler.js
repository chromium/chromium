// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to handle keyboard input.
 */
class KeyboardHandler {
  /**
   * @param {!SwitchAccessInterface} switchAccess
   */
  constructor(switchAccess) {
    /**
     * SwitchAccess reference.
     * @private {!SwitchAccessInterface}
     */
    this.switchAccess_ = switchAccess;

    this.init_();
  }

  /**
   * Update the keyboard keys captured by Switch Access to those stored in
   * prefs.
   */
  updateSwitchAccessKeys() {
    let keyCodes = [];
    for (const command of this.switchAccess_.getCommands()) {
      const keyCode = this.keyCodeFor_(command);
      if ((keyCode >= '0'.charCodeAt(0) && keyCode <= '9'.charCodeAt(0)) ||
          (keyCode >= 'A'.charCodeAt(0) && keyCode <= 'Z'.charCodeAt(0)))
        keyCodes.push(keyCode);
    }
    chrome.accessibilityPrivate.setSwitchAccessKeys(keyCodes);
  }

  /**
   * Run the command associated with the passed keyboard event.
   *
   * @param {!Event} event
   * @private
   */
  handleKeyEvent_(event) {
    for (const command of this.switchAccess_.getCommands()) {
      if (this.keyCodeFor_(command) === event.keyCode) {
        this.switchAccess_.runCommand(command);
        this.switchAccess_.performedUserAction();
        return;
      }
    }
  }

  /**
   * Set up key listener.
   * @private
   */
  init_() {
    this.updateSwitchAccessKeys();
    document.addEventListener('keyup', this.handleKeyEvent_.bind(this));
  }

  /**
   * Return the key code that |command| maps to.
   *
   * @param {string} command
   * @return {number}
   */
  keyCodeFor_(command) {
    return this.switchAccess_.getNumberPref(command);
  }
}
