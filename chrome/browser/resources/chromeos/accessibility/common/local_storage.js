// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Class to handle accessing/storing/caching local storage data.
 */

export class LocalStorage {
  /** @param {function(LocalStorage)} onInit */
  constructor(onInit) {
    /** @private {?Object} */
    this.values_ = null;

    chrome.storage.local.get(
        null /* get all values */,
        values => this.onInitialGet_(values, onInit));
    chrome.storage.local.onChanged.addListener(
        updates => this.update_(updates));
  }

  // ========== Static methods ==========

  static async init() {
    if (LocalStorage.instance) {
      throw new Error(
          'LocalStorage.init() should be called at most once in each ' +
          'browser context.');
    }

    LocalStorage.instance =
        await new Promise(resolve => new LocalStorage(resolve));
    LocalStorage.migrateFromLocalStorage_();

    return;
  }

  /** @param {string} key */
  static get(key) {
    LocalStorage.assertReady_();
    return LocalStorage.instance.values_[key];
  }

  /** @param {string} key */
  static remove(key) {
    LocalStorage.assertReady_();
    chrome.storage.local.remove(key);
    delete LocalStorage.instance.values_[key];
  }

  /**
   * @param {string} key
   * @param {*} val
   */
  static set(key, val) {
    LocalStorage.assertReady_();
    chrome.storage.local.set({[key]: val});
    LocalStorage.instance.values_[key] = val;
  }

  /** @param {string} key */
  static toggle(key) {
    LocalStorage.assertReady_();
    const val = LocalStorage.get(key);
    if (typeof val !== 'boolean') {
      throw new Error('Cannot toggle value of non-boolean setting');
    }
    LocalStorage.set(key, !val);
  }

  // ========= Private Methods ==========

  /**
   * @param {!Object} values
   * @param {function(LocalStorage)} onInit
   * @private
   */
  onInitialGet_(values, onInit) {
    this.values_ = values;
    onInit(this);
  }

  /**
   * @param {!Object<string, {newValue: *}>} updates
   * @private
   */
  update_(updates) {
    for (const key in updates) {
      this.values_[key] = updates[key].newValue;
    }
  }

  /** @private */
  static migrateFromLocalStorage_() {
    // Save the keys, because otherwise the values are shifting under us as we
    // iterate.
    const keys = [];
    for (let i = 0; i < localStorage.length; i++) {
      keys.push(localStorage.key(i));
    }

    for (const key of keys) {
      let val = localStorage[key];
      delete localStorage[key];

      if (val === String(true)) {
        val = true;
      } else if (val === String(false)) {
        val = false;
      } else if (/^\d+$/.test(val)) {
        // A string that with at least one digit and no other characters is an
        // integer.
        val = parseInt(val, 10);
      } else if (/^[\d]+[.][\d]+$/.test(val)) {
        // A string with at least one digit, followed by a dot, followed by at
        // least one digit is a floating point number.
        //
        // When converting floats to strings, v8 adds the leading 0 if there
        // were no digits before the decimal. E.g. String(.2) === "0.2"
        //
        // Similarly, integer values followed by a dot and any number of zeroes
        // are stored without a decimal and will be handled by the above case.
        // E.g. String(1.0) === "1"
        val = parseFloat(val);
      } else if (/^{.*}$/.test(val) || /^\[.*]$/.test(val)) {
        // If a string begins and ends with curly or square brackets, try to
        // convert it to an object/array. JSON.parse() will throw an error if
        // the string is not valid JSON syntax. In that case, the variable value
        // will remain unchanged (with a type of 'string').
        try {
          val = JSON.parse(val);
        } catch (syntaxError) {
        }
      }

      // We cannot call LocalStorage.set() because assertReady will fail.
      chrome.storage.local.set({[key]: val});
      LocalStorage.instance.values_[key] = val;
    }
  }

  /** @private */
  static assertReady_() {
    if (!LocalStorage.instance || !LocalStorage.instance.values_) {
      throw new Error(
          'LocalStorage should not be accessed until initialization is ' +
          'complete.');
    }
  }
}

/** @private {!LocalStorage} */
LocalStorage.instance;
