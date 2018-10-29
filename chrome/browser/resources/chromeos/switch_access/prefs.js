// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to manage user preferences.
 */
class SwitchAccessPrefs {
  /**
   * @param {!SwitchAccessInterface} switchAccess
   */
  constructor(switchAccess) {
    /**
     * SwitchAccess reference.
     * @private {!SwitchAccessInterface}
     */
    this.switchAccess_ = switchAccess;

    /**
     * User preferences, initially set to the default preference values.
     * @private
     */
    this.prefs_ = Object.assign({}, DEFAULT_PREFS);

    this.init_();
  }

  /**
   * Store any changes from chrome.storage.sync to this.prefs_, if this.prefs_
   * is not already set to that value. If this.prefs_ changes, fire a
   * prefsUpdate event.
   *
   * @param {!Object} storageChanges
   * @param {string} areaName
   * @private
   */
  handleStorageChange_(storageChanges, areaName) {
    let updatedPrefs = {};
    for (const key of Object.keys(storageChanges)) {
      if (this.prefs_[key] !== storageChanges[key].newValue) {
        this.prefs_[key] = storageChanges[key].newValue;
        updatedPrefs[key] = storageChanges[key].newValue;
      }
    }
    if (Object.keys(updatedPrefs).length > 0) {
      let event = new CustomEvent('prefsUpdate', {'detail': updatedPrefs});
      document.dispatchEvent(event);
    }
  }

  /**
   * @private
   */
  init_() {
    for (const command of this.switchAccess_.getCommands())
      this.prefs_[command] = this.switchAccess_.getDefaultKeyCodeFor(command);

    this.loadPrefs_();
    chrome.storage.onChanged.addListener(this.handleStorageChange_.bind(this));
  }

  /**
   * Asynchronously load the current preferences from chrome.storage.sync and
   * store them in this.prefs_, if this.prefs_ is not already set to that value.
   * If this.prefs_ changes, fire a prefsUpdate event.
   *
   * @private
   */
  loadPrefs_() {
    const defaultKeys = Object.keys(this.prefs_);
    chrome.storage.sync.get(defaultKeys, (loadedPrefs) => {
      let updatedPrefs = {};

      for (const key of Object.keys(loadedPrefs)) {
        if (this.prefs_[key] !== loadedPrefs[key]) {
          this.prefs_[key] = loadedPrefs[key];
          updatedPrefs[key] = loadedPrefs[key];
        }
      }

      if (Object.keys(updatedPrefs).length > 0) {
        const event = new CustomEvent('prefsUpdate', {'detail': updatedPrefs});
        document.dispatchEvent(event);
      }
    });
  }

  /**
   * Set the value of the preference |key| to |value| in chrome.storage.sync.
   * this.prefs_ is not set until handleStorageChange_.
   *
   * @param {string} key
   * @param {boolean|string|number} value
   */
  setPref(key, value) {
    let pref = {};
    pref[key] = value;
    chrome.storage.sync.set(pref);
  }

  /**
   * Get the value of type 'boolean' of the preference |key|. Will throw a type
   * error if the value of |key| is not 'boolean'.
   *
   * @param  {string} key
   * @return {boolean}
   */
  getBooleanPref(key) {
    const value = this.prefs_[key];
    if (typeof value === 'boolean')
      return value;
    else
      throw new TypeError('No value of boolean type for key \'' + key + '\'');
  }

  /**
   * Get the value of type 'number' of the preference |key|. Will throw a type
   * error if the value of |key| is not 'number'.
   *
   * @param  {string} key
   * @return {number}
   */
  getNumberPref(key) {
    const value = this.prefs_[key];
    if (typeof value === 'number')
      return value;
    else
      throw new TypeError('No value of number type for key \'' + key + '\'');
  }

  /**
   * Get the value of type 'string' of the preference |key|. Will throw a type
   * error if the value of |key| is not 'string'.
   *
   * @param  {string} key
   * @return {string}
   */
  getStringPref(key) {
    const value = this.prefs_[key];
    if (typeof value === 'string')
      return value;
    else
      throw new TypeError('No value of string type for key \'' + key + '\'');
  }

  /**
   * Returns true if |keyCode| is already used to run a command from the
   * keyboard.
   *
   * @param {number} keyCode
   * @return {boolean}
   */
  keyCodeIsUsed(keyCode) {
    for (const command of this.switchAccess_.getCommands()) {
      if (keyCode === this.prefs_[command])
        return true;
    }
    return false;
  }
}

/**
 * The default value of all preferences besides command keyboard bindings.
 * All preferences should be primitives to prevent changes to default values.
 */
const DEFAULT_PREFS = {
  'enableAutoScan': false,
  'autoScanTime': 800
};
