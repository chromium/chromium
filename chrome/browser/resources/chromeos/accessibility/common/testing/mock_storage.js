// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * A mock chrome.storage API for tests.
 */
const MockStorage = {

  /** @type {Object<string, *>} */
  local_: {},
  /** @type {Object<string, *>} */
  sync_: {},

  /** @type {Array<function(Object<string, *>)>} */
  callbacks_: [],

  local: {
    /**
     * @param {Object<string, string>} updates Map from keys to values to store.
     */
    set: updates => {
      Object.keys(updates).forEach(key => {
        MockStorage.local_[key] = updates[key];
      });
      MockStorage.callOnChangedListeners(this.local_);
    },

    /**
     * @param {Array<string>} unused The keys to return.
     * @param {function(Object<string, *>)} callback The callback to send
     *     values to.
     */
    get: (unused, callback) => {
      // Just send all the values for testing.
      callback(MockStorage.local_);
    },

    /**
     * Removes the value with the given key.
     * @param {string} key The key to remove.
     */
    remove: key => {
      delete MockStorage.local_[key];
      MockStorage.callOnChangedListeners(this.local_);
    },
  },

  sync: {
    /**
     * @param {Object<string, *>} updates Map from keys to values to store.
     */
    set: updates => {
      Object.keys(updates).forEach(key => {
        MockStorage.sync_[key] = updates[key];
      });
      MockStorage.callOnChangedListeners(this.sync_);
    },

    /**
     * @param {Array<string>} unused The keys to return.
     * @param {function(Object<string, *>)} callback The callback to send
     *     values to.
     */
    get: (unused, callback) => {
      // Just send all the values for testing.
      callback(MockStorage.sync_);
    },

    /**
     * Removes the value with the given key.
     * @param {string} key The key to remove.
     */
    remove: key => {
      delete MockStorage.sync_[key];
      MockStorage.callOnChangedListeners(this.sync_);
    },
  },

  onChanged: {
    /**
     * Set the onChanged callback.
     * @param {function(Object<string, *>)}
     */
    addListener: callback => {
      MockStorage.callbacks_.push(callback);
    },
  },

  /**
   * Calls onChanged listeners as if values have been updated.
   * This is functionality for testing and not part of the API.
   * @param {!Object<string, *>} opt_values
   */
  callOnChangedListeners: opt_values => {
    MockStorage.callbacks_.forEach(callback => {
      const baseObject = opt_values || MockStorage.sync_;
      const result = {};
      for (const key in baseObject) {
        result[key] = {newValue: baseObject[key]};
      }
      callback(result);
    });
  },

  /**
   * Clears the current values.
   * This is functionality for testing and not part of the API.
   */
  clear: () => {
    MockStorage.local_ = {};
    MockStorage.sync_ = {};
    MockStorage.callbacks_ = [];
  },
};
