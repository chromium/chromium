// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Discover OOBE/Login screen.
 */

login.createScreen('DiscoverScreen', 'discover', function() {
  return {
    /**
     * Returns the control which should receive initial focus.
     */
    get defaultControl() {
      return $('discover-impl');
    },

    /** Initial UI State for screen */
    getOobeUIInitialState() {
      return OOBE_UI_STATE.ONBOARDING;
    },

    /**
     * Event handler that is invoked just before the screen is shown.
     * @param {object} data Screen init payload.
     */
    onBeforeShow(data) {
      $('discover-impl').addEventListener('discover-done', function() {
        chrome.send('login.PinSetupScreen.userActed', ['finished']);
      });
    },

    /**
     * This is called after resources are updated.
     */
    updateLocalizedContent() {
      $('discover-impl').updateLocalizedContent();
    },
  };
});
