// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(function() {


/**
 * UI mode for the dialog.
 * @enum {string}
 */
const UIState = {
  PASSWORD: 'password',
  FORGOT: 'forgot',
  PROGRESS: 'progress',
};

Polymer({
  is: 'gaia-password-changed-element',

  behaviors: [
    OobeI18nBehavior,
    LoginScreenBehavior,
    MultiStepBehavior,
  ],

  properties: {
    email: {
      type: String,
      value: '',
    },

    password_: {
      type: String,
      value: '',
    },

    passwordInvalid_: {
      type: Boolean,
      value: false,
    },

    disabled: {type: Boolean, value: false},
  },

  defaultUIStep() {
    return UIState.PASSWORD;
  },

  UI_STEPS: UIState,

  /** @override */
  ready() {
    this.initializeLoginScreen('GaiaPasswordChangedScreen', {
      resetAllowed: false,
    });

    cr.ui.LoginUITools.addSubmitListener(
        this.$.oldPasswordInput, this.submit_.bind(this));
  },

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.PASSWORD_CHANGED;
  },

  // Invoked just before being shown. Contains all the data for the screen.
  onBeforeShow(data) {
    this.reset();
    this.email = data && 'email' in data && data.email;
    this.passwordInvalid_ = data && 'showError' in data && data.showError;
  },

  reset() {
    this.setUIStep(UIState.PASSWORD);
    this.clearPassword();
    this.disabled = false;
  },

  /** @private */
  submit_() {
    if (this.disabled)
      return;
    if (!this.$.oldPasswordInput.validate())
      return;
    this.setUIStep(UIState.PROGRESS);
    this.disabled = true;

    chrome.send('migrateUserData', [this.$.oldPasswordInput.value]);
  },

  /** @private */
  onForgotPasswordClicked_() {
    this.setUIStep(UIState.FORGOT);
    this.clearPassword();
  },

  /** @private */
  onTryAgainClicked_() {
    this.setUIStep(UIState.PASSWORD);
  },

  /** @private */
  onAnimationFinish_() {
    this.focus();
  },

  clearPassword() {
    this.password_ = '';
    this.passwordInvalid_ = false;
  },

  /** @private */
  onProceedClicked_() {
    if (this.disabled)
      return;
    this.setUIStep(UIState.PROGRESS);
    this.disabled = true;
    this.clearPassword();
    this.userActed('resync');
  },

  /** @private */
  onCancel_() {
    if (this.disabled)
      return;
    this.userActed('cancel');
  }
});
})();
