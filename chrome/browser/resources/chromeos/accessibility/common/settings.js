// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Class to handle accessing/storing/caching prefs data.
 */
const PrefObject = chrome.settingsPrivate.PrefObject;

export class Settings {
  /**
   * @param {!Array<string>} keys The settings keys the extension cares about.
   * @private
   */
  constructor(keys) {
    /** @private {Object<string, !Array<!Function>>} */
    this.listeners_ = {};
    /** @private {?Object<string, PrefObject>} */
    this.prefs_ = null;

    keys.forEach(key => this.listeners_[key] = []);
    chrome.settingsPrivate.onPrefsChanged.addListener(
        updates => this.update_(updates));
  }

  /**
   * @param {!Array<string>} keys The settings keys the extension cares about.
   */
  static async init(keys) {
    if (Settings.instance) {
      throw new Error(
          'Settings.init() should be called at most once in each ' +
          'browser context.');
    }

    Settings.instance = new Settings(keys);
    await Settings.instance.initialFetch_();
  }

  /**
   * Adds a callback to listen to changes to one or more preferences.
   * The callback will be called immediately if there is a value set.
   * @param {string|!Array<string>} keys The settings keys being listened to.
   * @param {!Function} listener The callback when the value changes.
   */
  static addListener(keys, listener) {
    if (typeof keys === 'string') {
      keys = [keys];
    }

    for (const key of keys) {
      Settings.instance.addListener_(key, listener);
    }
  }

  /**
   * @param {string} key
   * @return {?PrefObject}
   */
  static get(key) {
    Settings.instance.validate_(key);
    return Settings.instance.prefs_[key];
  }

  // ============ Private methods ============

  /**
   * @param {string} key The settings key being listened to.
   * @param {!Function} listener The callback when the value changes.
   * @private
   */
  addListener_(key, listener) {
    this.validate_(key);
    this.listeners_[key].push(listener);

    if (this.prefs_[key] !== null) {
      listener(this.prefs_[key]);
    }
  }

  /** @private */
  async initialFetch_() {
    const prefs = await new Promise(
        resolve => chrome.settingsPrivate.getAllPrefs(resolve));
    this.prefs_ = prefs.filter(pref => this.isTracked_(pref.key));
  }

  /**
   * @param {string} key
   * @private
   */
  isTracked_(key) {
    // Because we assign to this.prefs_ in initialFetch_(), use listeners_ as
    // the official source of truth on what keys are in scope.
    return key in this.listeners_;
  }

  /**
   * @param {!Array<!PrefObject>} updates
   * @private
   */
  update_(updates) {
    for (const pref of updates) {
      if (!this.isTracked_(pref.key)) {
        continue;
      }

      const oldValue = this.prefs_[pref.key].value;
      if (oldValue === pref.value) {
        continue;
      }

      this.prefs_[pref.key] = pref;
      this.listeners_[pref.key].forEach(listener => listener(pref.value));
    }
  }

  /**
   * @param {string} key
   * @private
   */
  validate_(key) {
    if (this.prefs_ === null) {
      throw new Error('Cannot access Settings until init() has resolved.');
    }
    if (!this.isTracked_(key)) {
      throw new Error('Prefs key "' + key + '" is not being tracked.');
    }
  }
}

/** @private {Settings} */
Settings.instance;
