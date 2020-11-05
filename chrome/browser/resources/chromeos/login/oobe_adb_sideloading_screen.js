// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying ARC ADB sideloading screen.
 */

'use strict';

(function() {

/**
 * UI mode for the dialog.
 * @enum {string}
 */
const UIState = {
  SETUP: 'setup',
  ERROR: 'error',
};

// The constants need to be synced with EnableAdbSideloadingScreenView::UIState.
const ADB_SIDELOADING_SCREEN_STATE = {
  ERROR: 1,
  SETUP: 2,
};

Polymer({
  is: 'oobe-adb-sideloading-element',

  behaviors: [
    OobeI18nBehavior,
    OobeDialogHostBehavior,
    LoginScreenBehavior,
    MultiStepBehavior,
  ],

  EXTERNAL_API: [
    'setScreenState',
  ],

  UI_STEPS: UIState,

  defaultUIStep() {
    return UIState.SETUP;
  },

  ready() {
    this.initializeLoginScreen('EnableAdbSideloadingScreen', {
      resetAllowed: true,
    });
  },

  /*
   * Executed on language change.
   */
  updateLocalizedContent() {
    this.i18nUpdateLocale();
  },

  onBeforeShow(data) {
    this.setScreenState(this.SCREEN_STATE_SETUP);
  },

  /**
   * Sets UI state for the dialog to show corresponding content.
   * @param {ADB_SIDELOADING_SCREEN_STATE} state.
   */
  setScreenState(state) {
    if (state == ADB_SIDELOADING_SCREEN_STATE.ERROR) {
      this.setUIStep(UIState.ERROR);
    } else if (state == ADB_SIDELOADING_SCREEN_STATE.SETUP) {
      this.setUIStep(UIState.SETUP);
    }
  },

  /**
   * On-tap event handler for enable button.
   *
   * @private
   */
  onEnableTap_() {
    this.userActed('enable-pressed');
  },

  /**
   * On-tap event handler for cancel button.
   *
   * @private
   */
  onCancelTap_() {
    this.userActed('cancel-pressed');
  },

  /**
   * On-tap event handler for learn more link.
   *
   * @private
   */
  onLearnMoreTap_() {
    this.userActed('learn-more-link');
  },
});
})();
