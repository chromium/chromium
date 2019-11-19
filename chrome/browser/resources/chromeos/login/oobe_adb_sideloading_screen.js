// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying ARC ADB sideloading screen.
 */

// The constants need to be synced with EnableAdbSideloadingScreenView::UIState.
const ADB_SIDELOADING_SCREEN_STATE = {
  ERROR: 1,
  SETUP: 2,
};

Polymer({
  is: 'oobe-adb-sideloading-screen',

  behaviors: [I18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],

  properties: {
    uiState_: String,
  },

  EXTERNAL_API: [
    'setScreenState',
  ],

  ready: function() {
    this.initializeLoginScreen('EnableAdbSideloadingScreen', {
      noAnimatedTransition: true,
      resetAllowed: true,
    });
    this.setScreenState(this.SCREEN_STATE_SETUP);
  },

  focus: function() {
    if (this.uiState_ === ADB_SIDELOADING_SCREEN_STATE.SETUP) {
      this.$.enableAdbSideloadDialog.focus();
    } else if (this.uiState_ === ADB_SIDELOADING_SCREEN_STATE.ERROR) {
      this.$.enableAdbSideloadErrorDialog.focus();
    }
  },

  onBeforeShow: function(data) {
    this.setScreenState(this.SCREEN_STATE_SETUP);
  },

  /**
   * Sets UI state for the dialog to show corresponding content.
   * @param {ADB_SIDELOADING_SCREEN_STATE} state.
   */
  setScreenState: function(state) {
    if (state == ADB_SIDELOADING_SCREEN_STATE.ERROR) {
      this.uiState_ = 'error';
    } else if (state == ADB_SIDELOADING_SCREEN_STATE.SETUP) {
      this.uiState_ = 'setup';
    }
  },

  isState_: function(uiState, state) {
    return uiState === state;
  },

  /**
   * On-tap event handler for enable button.
   *
   * @private
   */
  onEnableTap_: function() {
    chrome.send(
        'login.EnableAdbSideloadingScreen.userActed', ['enable-pressed']);
  },

  /**
   * On-tap event handler for cancel button.
   *
   * @private
   */
  onCancelTap_: function() {
    chrome.send(
        'login.EnableAdbSideloadingScreen.userActed', ['cancel-pressed']);
  },


  /**
   * On-tap event handler for learn more link.
   *
   * @private
   */
  onLearnMoreTap_: function() {
    chrome.send(
        'login.EnableAdbSideloadingScreen.userActed', ['learn-more-link']);
  },
});
