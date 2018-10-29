// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Demo Setup
 * screen.
 */

Polymer({
  is: 'demo-setup-md',

  behaviors: [I18nBehavior, OobeDialogHostBehavior],

  properties: {
    /** Error message displayed on demoSetupErrorDialog screen. */
    errorMessage_: {
      type: String,
      value: '',
    },

    /** Ordered array of screen ids that are a part of demo setup flow. */
    screens_: {
      type: Array,
      readonly: true,
      value: function() {
        return ['demoSetupProgressDialog', 'demoSetupErrorDialog'];
      },
    },
  },

  /** Resets demo setup flow to the initial screen and starts setup. */
  reset: function() {
    this.showScreen_('demoSetupProgressDialog');
    chrome.send('login.DemoSetupScreen.userActed', ['start-setup']);
  },

  /** Called after resources are updated. */
  updateLocalizedContent: function() {
    this.i18nUpdateLocale();
  },

  /** Called when demo mode setup succeeded. */
  onSetupSucceeded: function() {
    this.errorMessage_ = '';
  },

  /**
   * Called when demo mode setup failed.
   * @param {string} message Error message to be displayed to the user.
   */
  onSetupFailed: function(message) {
    this.errorMessage_ = message;
    this.showScreen_('demoSetupErrorDialog');
  },

  /**
   * Shows screen with the given id. Method exposed for testing environment.
   * @param {string} id Screen id.
   */
  showScreenForTesting: function(id) {
    this.showScreen_(id);
  },

  /**
   * Shows screen with the given id.
   * @param {string} id Screen id.
   * @private
   */
  showScreen_: function(id) {
    this.hideScreens_();

    var screen = this.$[id];
    assert(screen);
    screen.hidden = false;
    screen.show();
  },

  /**
   * Hides all screens to help switching from one screen to another.
   * @private
   */
  hideScreens_: function() {
    for (let id of this.screens_) {
      var screen = this.$[id];
      assert(screen);
      screen.hidden = true;
    }
  },

  /**
   * Retry button click handler.
   * @private
   */
  onRetryClicked_: function() {
    this.reset();
  },

  /**
   * Close button click handler.
   * @private
   */
  onCloseClicked_: function() {
    chrome.send('login.DemoSetupScreen.userActed', ['close-setup']);
  },
});
