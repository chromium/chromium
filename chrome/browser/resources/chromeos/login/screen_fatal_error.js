// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A simple message box screen implementation.
 */

login.createScreen('FatalErrorScreen', 'fatal-error', function() {
  return {
    EXTERNAL_API: ['show'],

    /**
     * Callback to run when the screen is dismissed.
     * @type {function()}
     */
    callback_: null,

    /**
     * Saved UI states to restore when this screen hides.
     * @type {Object}
     */
    savedUIStates_: {},

    /** @override */
    decorate: function() {
      $('fatal-error-card')
          .addEventListener('buttonclick', this.onDismiss_.bind(this));
    },

    /** @override */
    get defaultControl() {
      return $('fatal-error-card').submitButton;
    },

    /**
     * Invoked when user clicks on the ok button.
     */
    onDismiss_: function() {
      this.callback_();
    },

    /**
     * Shows the fatal error string screen.
     * @param {string} message The error message to show.
     * @param {function()} callback The callback to be invoked when the
     *     screen is dismissed.
     */
    show: function(message, buttonLabel, callback) {
      $('fatal-error-card').textContent = message;
      $('fatal-error-card').buttonLabel = buttonLabel;
      this.callback_ = callback;
      Oobe.showScreen({id: SCREEN_FATAL_ERROR});
    }
  };
});
