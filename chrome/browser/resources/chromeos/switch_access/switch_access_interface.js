// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Interface for controllers to interact with main SwitchAccess class.
 * @interface
 */
class SwitchAccessInterface {
  /**
   * Open and jump to the Switch Access menu.
   */
  enterMenu() {}

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
   * Restarts auto-scan if it is enabled.
   */
  restartAutoScan() {}

  /**
   * Sets whether the current node is in the virtual keyboard.
   * @param {boolean} inKeyboard
   */
  setInKeyboard(inKeyboard) {}

  /**
   * Check whether or not the feature flag
   * for improved text input is enabled.
   * @return {boolean}
   */
  improvedTextInputEnabled() {}

  /**
   * Handle a change in user preferences.
   * @param {!Object} changes
   */
  onPreferencesChanged(changes) {}

  /**
   * Returns whether prefs have initially loaded or not.
   * @return {boolean}
   */
  prefsAreReady() {}

  /**
   * Set the value of the preference |name| to |value| in chrome.storage.sync.
   * The behavior is not updated until the storage update is complete.
   *
   * @param {SAConstants.Preference} name
   * @param {boolean|number} value
   */
  setPreference(name, value) {}

  /**
   * Get the boolean value for the given name. Will throw an error if the
   * value associated with |name| is not a boolean, or undefined.
   *
   * @param  {SAConstants.Preference} name
   * @return {boolean}
   */
  getBooleanPreference(name) {}

  /**
   * Get the string value for the given name. Will throw an error if the
   * value associated with |name| is not a string, or is undefined.
   *
   * @param {SAConstants.Preference} name
   * @return {string}
   */
  getStringPreference(name) {}

  /**
   * Get the number value for the given name. Will throw an error if the
   * value associated with |name| is not a number, or undefined.
   *
   * @param  {SAConstants.Preference} name
   * @return {number}
   */
  getNumberPreference(name) {}

  /**
   * Get the number value for the given name, or |null| if none exists.
   *
   * @param  {SAConstants.Preference} name
   * @return {number|null}
   */
  getNumberPreferenceIfDefined(name) {}

  /**
   * Sets up the connection between the menuPanel and the menuManager.
   * @param {!PanelInterface} menuPanel
   * @return {MenuManager}
   */
  connectMenuPanel(menuPanel) {}
}
