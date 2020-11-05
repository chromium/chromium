// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for Parental Handoff screen.
 */

Polymer({
  is: 'parental-handoff-element',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],

  properties: {
    /**
     * THe title to be displayed
     */
    title_: {
      type: String,
      value: '',
    },

    /**
     * The subtitle to be displayed.
     */
    subtitle_: {
      type: String,
      value: '',
    },
  },

  /**
   * Event handler that is invoked just before the frame is shown.
   * @param {Object} data Screen init payload
   */
  onBeforeShow(data) {
    if ('title' in data) {
      this.title_ = data.title;
    }

    if ('subtitle' in data) {
      this.subtitle_ = data.subtitle;
    }
  },

  ready() {
    this.initializeLoginScreen('ParentalHandoffScreen', {
      resetAllowed: true,
    });
  },

  /*
   * Executed on language change.
   */
  updateLocalizedContent() {
    this.i18nUpdateLocale();
  },

  /**
   * On-tap event handler for Next button.
   *
   * @private
   */
  onNextButtonPressed_() {
    this.userActed('next');
  },

});
