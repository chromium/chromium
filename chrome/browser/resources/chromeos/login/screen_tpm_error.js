// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Offline message screen implementation.
 */

Polymer({
  is: 'tpm-error-message-element',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],

  ready() {
    this.initializeLoginScreen('TPMErrorMessageScreen', {
      resetAllowed: true,
    });
  },

  onRestartTap_() {
    this.userActed('reboot-system');
  },

  /**
   * Returns default event target element.
   * @type {Object}
   */
  get defaultControl() {
    return this.$.errorDialog;
  },
});
