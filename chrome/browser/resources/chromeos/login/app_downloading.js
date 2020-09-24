// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design App Downloading
 * screen.
 */

Polymer({
  is: 'app-downloading',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],

  properties: {
    numOfApps: Number,

    /** Whether the user selected one app. */
    hasSingleApp: {
      type: Boolean,
      computed: 'hasSingleApp_(numOfApps)',
    },
  },

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
  onBeforeShow(data) {
    this.numOfApps = data.numOfApps;
    if (this.$.video) {
      this.$.video.play();
    }
  },

  /** Called when dialog is hidden */
  onBeforeHide() {
    if (this.$.video) {
      this.$.video.pause();
    }
  },

  /** @private */
  onContinue_() {
    this.userActed('appDownloadingContinueSetup');
  },

  /** @private */
  hasSingleApp_(numOfApps) {
    return numOfApps === 1;
  },

  /** @private */
  getDialogTitleA11yString_(numOfApps) {
    if (this.hasSingleApp_(numOfApps)) {
      return this.i18n('appDownloadingScreenTitleSingular');
    } else {
      return this.i18n('appDownloadingScreenTitlePlural', numOfApps);
    }
  },
});
