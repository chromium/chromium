// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for Packaged License screen.
 */

Polymer({
  is: 'packaged-license-element',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],

  properties: {

  },

  ready() {
    this.initializeLoginScreen('PackagedLicenseScreen', {
      resetAllowed: true,
    });
  },

  /**
   * Returns the control which should receive initial focus.
   */
  get defaultControl() {
    return this.$.packagedLicenseDialog;
  },

  /*
   * Executed on language change.
   */
  updateLocalizedContent() {
    this.i18nUpdateLocale();
  },

  /**
   * On-tap event handler for Don't Enroll button.
   *
   * @private
   */
  onDontEnrollButtonPressed_() {
    this.userActed('dont-enroll');
  },

  /**
   * On-tap event handler for Enroll button.
   *
   * @private
   */
  onEnrollButtonPressed_() {
    this.userActed('enroll');
  },

});
