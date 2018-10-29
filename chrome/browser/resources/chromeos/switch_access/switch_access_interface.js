// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Interface for controllers to interact with main SwitchAccess class.
 * @interface
 */
class SwitchAccessInterface {
  /**
   * Jump to the context menu.
   */
  enterContextMenu() {}

  /**
   * Move to the next interesting node.
   */
  moveForward() {}

  /**
   * Move to the previous interesting node.
   */
  moveBackward() {}

  /**
   * Perform the default action on the current node.
   */
  selectCurrentNode() {}

  /**
   * Open the options page in a new tab.
   */
  showOptionsPage() {}

  /**
   * Return a list of the names of all user commands.
   * @return {!Array<string>}
   */
  getCommands() {}

  /**
   * Return the default key code for a command.
   *
   * @param {string} command
   * @return {number}
   */
  getDefaultKeyCodeFor(command) {}

  /**
   * Run the function binding for the specified command.
   * @param {string} command
   */
  runCommand(command) {}

  /**
   * Perform actions as the result of actions by the user. Currently, restarts
   * auto-scan if it is enabled.
   */
  performedUserAction() {}

  /**
   * Set the value of the preference |key| to |value| in chrome.storage.sync.
   * this.prefs_ is not set until handleStorageChange_.
   *
   * @param {string} key
   * @param {boolean|string|number} value
   */
  setPref(key, value) {}

  /**
   * Get the value of type 'boolean' of the preference |key|. Will throw a type
   * error if the value of |key| is not 'boolean'.
   *
   * @param  {string} key
   * @return {boolean}
   */
  getBooleanPref(key) {}

  /**
   * Get the value of type 'number' of the preference |key|. Will throw a type
   * error if the value of |key| is not 'number'.
   *
   * @param  {string} key
   * @return {number}
   */
  getNumberPref(key) {}

  /**
   * Get the value of type 'string' of the preference |key|. Will throw a type
   * error if the value of |key| is not 'string'.
   *
   * @param  {string} key
   * @return {string}
   */
  getStringPref(key) {}

  /**
   * Returns true if |keyCode| is already used to run a command from the
   * keyboard.
   *
   * @param {number} keyCode
   * @return {boolean}
   */
  keyCodeIsUsed(keyCode) {}
}
