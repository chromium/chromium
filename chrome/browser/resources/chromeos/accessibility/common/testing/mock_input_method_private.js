// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * A mock chrome.inputMethodPrivate API for tests.
 */
var MockInputMethodPrivate = {
  /** @private {string} */
  currentInputMethod_: '',

  // Methods from chrome.inputMethodPrivate API. //

  /**
   * Gets the current input method.
   * @param {function<string>} callback
   */
  getCurrentInputMethod(callback) {
    callback(this.currentInputMethod_);
  },


  /**
   * Sets the current input method.
   * @param {string} inputMethodId The input method to set.
   * @param {function<>} callback Callback called on success.
   */
  setCurrentInputMethod(inputMethodId, callback) {
    MockInputMethodPrivate.currentInputMethod_ = inputMethodId;
    if (callback) {
      callback();
    }
  },

  // Methods for testing. //

  /**
   * Gets the current input method.
   * @return {string}
   */
  getCurrentInputMethodForTest() {
    return MockInputMethodPrivate.currentInputMethod_;
  },
};
