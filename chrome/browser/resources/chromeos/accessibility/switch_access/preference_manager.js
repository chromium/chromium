// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AutoScanManager} from './auto_scan_manager.js';

/**
 * Class to manage user preferences.
 */
export class PreferenceManager {
  /** @private */
  constructor() {
    /**
     * User preferences, initially set to the default preference values.
     * @private {!Map<Prefs,
     *                chrome.settingsPrivate.PrefObject>}
     */
    this.preferences_ = new Map();

    this.init_();
  }

  // =============== Static Methods ==============

  static initialize() {
    PreferenceManager.instance = new PreferenceManager();
  }

  // =============== Private Methods ==============

  /**
   * Get the boolean value for the given name, or |null| if the value is not a
   * boolean or does not exist.
   *
   * @param  {Prefs} name
   * @return {boolean|null}
   * @private
   */
  getBoolean_(name) {
    const pref = this.preferences_.get(name);
    if (pref && pref.type === chrome.settingsPrivate.PrefType.BOOLEAN) {
      return /** @type {boolean} */ (pref.value);
    } else {
      return null;
    }
  }

  /**
   * Get the number value for the given name, or |null| if the value is not a
   * number or does not exist.
   *
   * @param {Prefs} name
   * @return {number|null}
   * @private
   */
  getNumber_(name) {
    const pref = this.preferences_.get(name);
    if (pref && pref.type === chrome.settingsPrivate.PrefType.NUMBER) {
      return /** @type {number} */ (pref.value);
    }
    return null;
  }

  /**
   * Get the preference value for the given name, or |null| if the value is not
   * a dictionary or does not exist.
   *
   * @param {Prefs} name
   * @return {Object|null}
   * @private
   */
  getDict_(name) {
    const pref = this.preferences_.get(name);
    if (pref && pref.type === chrome.settingsPrivate.PrefType.DICTIONARY) {
      return /** @type {Object} */ (pref.value);
    }
    return null;
  }

  /** @private */
  init_() {
    chrome.settingsPrivate.onPrefsChanged.addListener(
        prefs => this.updateFromSettings_(prefs));
    chrome.settingsPrivate.getAllPrefs(
        prefs => this.updateFromSettings_(prefs, true /* isFirstLoad */));
  }

  /**
   * Whether the current settings configuration is reasonably usable;
   * specifically, whether there is a way to select and a way to navigate.
   * @return {boolean}
   * @private
   */
  settingsAreConfigured_() {
    const selectPref = this.getDict_(Prefs.SELECT_DEVICE_KEY_CODES);
    const selectSet = selectPref ? Object.keys(selectPref).length : false;

    const nextPref = this.getDict_(Prefs.NEXT_DEVICE_KEY_CODES);
    const nextSet = nextPref ? Object.keys(nextPref).length : false;

    const previousPref = this.getDict_(Prefs.PREVIOUS_DEVICE_KEY_CODES);
    const previousSet = previousPref ? Object.keys(previousPref).length : false;

    const autoScanEnabled =
        // getBoolean_() returns null if a value is not found, so we force the
        // value to be a boolean (defaulting to false).
        Boolean(this.getBoolean_(Prefs.AUTO_SCAN_ENABLED));

    if (!selectSet) {
      return false;
    }

    if (nextSet || previousSet) {
      return true;
    }

    return autoScanEnabled;
  }

  /**
   * Updates the cached preferences.
   * @param {!Array<chrome.settingsPrivate.PrefObject>} preferences
   * @param {boolean} isFirstLoad
   * @private
   */
  updateFromSettings_(preferences, isFirstLoad = false) {
    for (const pref of preferences) {
      // Ignore preferences that are not used by Switch Access.
      if (!this.usesPreference_(pref)) {
        continue;
      }

      const key = /** @type {Prefs} */ (pref.key);
      const oldPrefObject = this.preferences_.get(key);
      if (!oldPrefObject || oldPrefObject.value !== pref.value) {
        this.preferences_.set(key, pref);
        switch (key) {
          case Prefs.AUTO_SCAN_ENABLED:
            if (pref.type === chrome.settingsPrivate.PrefType.BOOLEAN) {
              AutoScanManager.setEnabled(/** @type {boolean} */ (pref.value));
            }
            break;
          case Prefs.AUTO_SCAN_TIME:
            if (pref.type === chrome.settingsPrivate.PrefType.NUMBER) {
              AutoScanManager.setPrimaryScanTime(
                  /** @type {number} */ (pref.value));
            }
            break;
          case Prefs.AUTO_SCAN_KEYBOARD_TIME:
            if (pref.type === chrome.settingsPrivate.PrefType.NUMBER) {
              AutoScanManager.setKeyboardScanTime(
                  /** @type {number} */ (pref.value));
            }
            break;
        }
      }
    }

    if (isFirstLoad && !this.settingsAreConfigured_()) {
      chrome.accessibilityPrivate.openSettingsSubpage(
          'manageAccessibility/switchAccess');
    }
  }

  /**
   * @param {!chrome.settingsPrivate.PrefObject} pref
   * @return {boolean}
   */
  usesPreference_(pref) {
    return Object.values(Prefs).includes(pref.key);
  }
}

/**
 * Preferences that are configurable in Switch Access.
 * @enum {string}
 */
const Prefs = {
  AUTO_SCAN_ENABLED: 'settings.a11y.switch_access.auto_scan.enabled',
  AUTO_SCAN_TIME: 'settings.a11y.switch_access.auto_scan.speed_ms',
  AUTO_SCAN_KEYBOARD_TIME:
      'settings.a11y.switch_access.auto_scan.keyboard.speed_ms',
  NEXT_DEVICE_KEY_CODES: 'settings.a11y.switch_access.next.device_key_codes',
  PREVIOUS_DEVICE_KEY_CODES:
      'settings.a11y.switch_access.previous.device_key_codes',
  SELECT_DEVICE_KEY_CODES:
      'settings.a11y.switch_access.select.device_key_codes',
};
