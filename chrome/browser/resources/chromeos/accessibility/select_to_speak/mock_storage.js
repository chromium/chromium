// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * A mock chrome.storage API for tests.
 */
const MockStorage = {

  /** @type {Object<string, string>} */
  prefs_: {},

  /** @type {Array<function(Object<string, string>)>} */
  callbacks_: [],

  sync: {
    /**
     * Sets preferences.
     * @param {Object<string, string>} prefs Map of pref key to pref value to
     *    save.
     */
    set: (prefs) => {
      Object.keys(prefs).forEach((key) => {
        MockStorage.prefs_[key] = prefs[key];
      });
      MockStorage.updatePrefs();
    },

    /**
     * Gets the current prefs list, for testing.
     * @param {Array<string>} unused The keys to return.
     * @param {function(Object<string, string>)} callback The callback to send
     *     prefs to.
     */
    get: (unused, callback) => {
      // Just send all the prefs for testing.
      callback(MockStorage.prefs_);
    },

    /**
     * Removes the pref with the given key.
     * @param {string} key The key to remove.
     */
    remove: (key) => {
      delete MockStorage.prefs_[key];
      MockStorage.updatePrefs();
    }
  },

  onChanged: {
    /**
     * Set the onChanged callback.
     * @param {function(Object<string, string>)}
     */
    addListener: (callback) => {
      MockStorage.callbacks_.push(callback);
    },
  },

  /**
   * Calls onChanged listeners as if prefs have been updated.
   * This is functionality for testing and not part of the API.
   */
  updatePrefs: () => {
    MockStorage.callbacks_.forEach((callback) => {
      callback(MockStorage.prefs_);
    });
  },

  /**
   * Clears the current prefs.
   * This is functionality for testing and not part of the API.
   */
  clear: () => {
    MockStorage.prefs_ = {};
    MockStorage.callbacks_ = [];
  }
};
