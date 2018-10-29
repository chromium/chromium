// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to manage the options page.
 */
class SwitchAccessOptions {
  constructor() {
    const background = chrome.extension.getBackgroundPage();

    /**
     * SwitchAccess reference.
     * @private {!SwitchAccessInterface}
     */
    this.switchAccess_ = background.switchAccess;

    this.init_();

    document.addEventListener('change', this.handleInputChange_.bind(this));
    background.document.addEventListener(
        'prefsUpdate', this.handlePrefsUpdate_.bind(this));
  }

  /**
   * Initialize the options page by setting all elements representing a user
   * preference to show the correct value.
   *
   * @private
   */
  init_() {
    document.getElementById('enableAutoScan').checked =
        this.switchAccess_.getBooleanPref('enableAutoScan');
    document.getElementById('autoScanTime').value =
        this.switchAccess_.getNumberPref('autoScanTime') / 1000;

    for (const command of this.switchAccess_.getCommands()) {
      document.getElementById(command).value =
          String.fromCharCode(this.switchAccess_.getNumberPref(command));
    }
  }

  /**
   * Handle a change by the user to an element representing a user preference.
   *
   * @param {!Event} event
   * @private
   */
  handleInputChange_(event) {
    const input = event.target;
    switch (input.id) {
      case 'enableAutoScan':
        this.switchAccess_.setPref(input.id, input.checked);
        break;
      case 'autoScanTime':
        const oldVal = this.switchAccess_.getNumberPref(input.id);
        const val = Number(input.value) * 1000;
        const min = Number(input.min) * 1000;
        if (this.isValidScanTimeInput_(val, oldVal, min)) {
          input.value = Number(input.value);
          this.switchAccess_.setPref(input.id, val);
        } else {
          input.value = oldVal;
        }
        break;
      default:
        if (this.switchAccess_.getCommands().includes(input.id)) {
          const keyCode = input.value.toUpperCase().charCodeAt(0);
          if (this.isValidKeyCode_(keyCode)) {
            input.value = input.value.toUpperCase();
            this.switchAccess_.setPref(input.id, keyCode);
          } else {
            const oldKeyCode = this.switchAccess_.getNumberPref(input.id);
            input.value = String.fromCharCode(oldKeyCode);
          }
        }
    }
  }

  /**
   * Return true if |keyCode| is a letter or number, and if it is not already
   * being used.
   *
   * @param {number} keyCode
   * @return {boolean}
   */
  isValidKeyCode_(keyCode) {
    return ((keyCode >= '0'.charCodeAt(0) && keyCode <= '9'.charCodeAt(0)) ||
            (keyCode >= 'A'.charCodeAt(0) && keyCode <= 'Z'.charCodeAt(0))) &&
        !this.switchAccess_.keyCodeIsUsed(keyCode);
  }

  /**
   * Return true if the input is a valid autoScanTime input. Otherwise, return
   * false.
   *
   * @param {number} value
   * @param {number} oldValue
   * @param {number} min
   * @return {boolean}
   */
  isValidScanTimeInput_(value, oldValue, min) {
    return (value !== oldValue) && (value >= min);
  }

  /**
   * Handle a change in user preferences.
   *
   * @param {!Event} event
   * @private
   */
  handlePrefsUpdate_(event) {
    const updatedPrefs = event.detail;
    for (let key of Object.keys(updatedPrefs)) {
      switch (key) {
        case 'enableAutoScan':
          document.getElementById(key).checked = updatedPrefs[key];
          break;
        case 'autoScanTime':
          document.getElementById(key).value = updatedPrefs[key] / 1000;
          break;
        default:
          if (this.switchAccess_.getCommands().includes(key))
            document.getElementById(key).value =
                String.fromCharCode(updatedPrefs[key]);
      }
    }
  }
}

document.addEventListener('DOMContentLoaded', function() {
  new SwitchAccessOptions();
});
