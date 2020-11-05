// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview wrong HWID screen implementation.
 */

Polymer({
  is: 'wrong-hwid-element',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],

  ready() {
    this.initializeLoginScreen('WrongHWIDMessageScreen', {
      resetAllowed: true,
    });
  },

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.WRONG_HWID_WARNING;
  },

  onSkip_() {
    this.userActed('skip-screen');
  },

  formattedFirstPart_(locale) {
    return this.i18nAdvanced('wrongHWIDMessageFirstPart');
  },
});
