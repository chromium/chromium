// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design App Downloading
 * screen.
 */

Polymer({
  is: 'app-downloading-element',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],

  properties: {},

  ready() {
    this.initializeLoginScreen('AppDownloadingScreen', {
      resetAllowed: true,
    });
  },

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.ONBOARDING;
  },

  /**
   * Returns the control which should receive initial focus.
   */
  get defaultControl() {
    return this.$['app-downloading-dialog'];
  },

  /*
   * Executed on language change.
   */
  updateLocalizedContent() {
    this.i18nUpdateLocale();
  },

  /** Called when dialog is shown */
  onBeforeShow() {
    if (this.$.downloadingApps) {
      this.$.downloadingApps.setPlay(true);
    }
  },

  /** Called when dialog is hidden */
  onBeforeHide() {
    if (this.$.downloadingApps) {
      this.$.downloadingApps.setPlay(false);
    }
  },

  /** @private */
  onContinue_() {
    this.userActed('appDownloadingContinueSetup');
  },
});
