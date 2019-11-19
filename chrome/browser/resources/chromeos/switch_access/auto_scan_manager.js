// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to handle auto-scan behavior.
 */
class AutoScanManager {
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
     * Auto-scan interval ID.
     * @private {number|undefined}
     */
    this.intervalID_;

    /**
     * Length of the default auto-scan interval (used wherever there is not
     * a more specific scan time set) in milliseconds.
     * @private {number}
     */
    this.defaultScanTime_ = SCAN_TIME_NOT_INITIALIZED;

    /**
     * Length of auto-scan interval for the on-screen keyboard in milliseconds.
     * @private {number}
     */
    this.keyboardScanTime_ = SCAN_TIME_NOT_INITIALIZED;

    /**
     * Whether the current node is within the virtual keyboard.
     * @private {boolean}
     */
    this.inKeyboard_ = false;
  }

  /** Finishes setup of auto scan manager once Prefs are loaded. */
  onPrefsReady() {
    this.defaultScanTime_ = this.switchAccess_.getNumberPreference(
        SAConstants.Preference.AUTO_SCAN_TIME);
    this.keyboardScanTime_ = this.switchAccess_.getNumberPreference(
        SAConstants.Preference.AUTO_SCAN_KEYBOARD_TIME);
    const enabled = this.switchAccess_.getBooleanPreference(
        SAConstants.Preference.AUTO_SCAN_ENABLED);
    if (enabled) {
      this.start_();
    }
  }

  /**
   * Return true if auto-scan is currently running. Otherwise return false.
   * @return {boolean}
   */
  isRunning() {
    return this.intervalID_ !== undefined;
  }

  /**
   * Restart auto-scan under the current settings if it is currently running.
   */
  restartIfRunning() {
    if (this.isRunning()) {
      this.stop_();
      this.start_();
    }
  }

  /**
   * Stop auto-scan if it is currently running. Then, if |enabled| is true,
   * turn on auto-scan. Otherwise leave it off.
   *
   * @param {boolean} enabled
   */
  setEnabled(enabled) {
    if (this.isRunning()) {
      this.stop_();
    }
    if (enabled) {
      this.start_();
    }
  }

  /**
   * Update this.defaultScanTime_ to |scanTime|. Then, if auto-scan is currently
   * running, restart it.
   *
   * @param {number} scanTime Auto-scan interval time in milliseconds.
   */
  setDefaultScanTime(scanTime) {
    this.defaultScanTime_ = scanTime;
    this.restartIfRunning();
  }

  /**
   * Update this.keyboardScanTime_ to |scanTime|.
   *
   * @param {number} scanTime Auto-scan interval time in milliseconds.
   */
  setKeyboardScanTime(scanTime) {
    this.keyboardScanTime_ = scanTime;
    if (this.inKeyboard_) {
      this.restartIfRunning();
    }
  }

  /**
   * Sets whether the keyboard scan time is used.
   * @param {boolean} inKeyboard
   */
  setInKeyboard(inKeyboard) {
    this.inKeyboard_ = inKeyboard;
  }

  /**
   * Stop the window from moving to the next node at a fixed interval.
   * @private
   */
  stop_() {
    window.clearInterval(this.intervalID_);
    this.intervalID_ = undefined;
  }

  /**
   * Set the window to move to the next node at an interval in milliseconds
   * depending on where the user is navigating. Currently,
   * this.keyboardScanTime_ is used as the interval if the user is
   * navigating in the virtual keyboard, and this.defaultScanTime_ is used
   * otherwise.
   * @private
   */
  start_() {
    if (this.defaultScanTime_ === SCAN_TIME_NOT_INITIALIZED) {
      return;
    }

    let currentScanTime = this.defaultScanTime_;

    if (this.switchAccess_.improvedTextInputEnabled() && this.inKeyboard_ &&
        this.keyboardScanTime_ !== SCAN_TIME_NOT_INITIALIZED) {
      currentScanTime = this.keyboardScanTime_;
    }

    this.intervalID_ = window.setInterval(
        this.switchAccess_.moveForward.bind(this.switchAccess_),
        currentScanTime);
  }
}

/**
 * Sentinel value that indicates an uninitialized scan time.
 * @type {number}
 */
const SCAN_TIME_NOT_INITIALIZED = -1;
