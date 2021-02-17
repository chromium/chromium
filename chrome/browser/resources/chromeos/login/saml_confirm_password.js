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
  PROGRESS: 'progress',
};

Polymer({
  is: 'saml-confirm-password-element',

  behaviors: [
    OobeI18nBehavior,
    LoginScreenBehavior,
    MultiStepBehavior,
  ],

  properties: {
    email: String,

    isManualInput: {
      type: Boolean,
      value: false,
    }
  },

  EXTERNAL_API: ['show'],

  defaultUIStep() {
    return UIState.PASSWORD;
  },

  UI_STEPS: UIState,

  /**
   * Callback to run when the screen is dismissed.
   * @type {?function(string)}
   */
  callback_: null,

  ready() {
    this.initializeLoginScreen('ConfirmSamlPasswordScreen', {
      resetAllowed: true,
    });

    cr.ui.LoginUITools.addSubmitListener(
        this.$.passwordInput, this.submit_.bind(this));
    cr.ui.LoginUITools.addSubmitListener(
        this.$.confirmPasswordInput, this.submit_.bind(this));
  },

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.SAML_PASSWORD_CONFIRM;
  },

  /**
   * Shows the confirm password screen.
   * @param {string} email The authenticated user's e-mail.
   * @param {boolean} manualPasswordInput True if no password has been
   *     scrapped and the user needs to set one manually for the device.
   * @param {number} attemptCount Number of attempts tried, starting at 0.
   * @param {function(string)} callback The callback to be invoked when the
   *     screen is dismissed.
   */
  show(email, manualPasswordInput, attemptCount, callback) {
    this.callback_ = callback;
    this.reset();
    this.email = email;
    this.isManualInput = manualPasswordInput;
    if (attemptCount > 0)
      this.$.passwordInput.invalid = true;
    cr.ui.Oobe.showScreen({id: 'saml-confirm-password'});
  },

  reset() {
    if (this.$.cancelConfirmDlg.open)
      this.$.cancelConfirmDlg.hideDialog();
    this.setUIStep(UIState.PASSWORD);
    this.$.passwordInput.invalid = false;
    this.$.passwordInput.value = '';
    if (this.isManualInput) {
      this.$$('#confirmPasswordInput').invalid = false;
      this.$$('#confirmPasswordInput').value = '';
    }
  },

  onCancel_() {
    this.$.cancelConfirmDlg.showDialog();
  },

  onCancelNo_() {
    this.$.cancelConfirmDlg.hideDialog();
  },

  onCancelYes_() {
    this.$.cancelConfirmDlg.hideDialog();

    cr.ui.Oobe.showScreen({id: 'gaia-signin'});
    cr.ui.Oobe.resetSigninUI(true);
  },

  submit_() {
    if (!this.$.passwordInput.validate())
      return;
    if (this.isManualInput) {
      // When using manual password entry, both passwords must match.
      var confirmPasswordInput = this.$$('#confirmPasswordInput');
      if (!confirmPasswordInput.validate())
        return;

      if (confirmPasswordInput.value != this.$.passwordInput.value) {
        this.$.passwordInput.invalid = true;
        confirmPasswordInput.invalid = true;
        return;
      }
    }

    this.setUIStep(UIState.PROGRESS);
    this.callback_(this.$.passwordInput.value);
    this.reset();
  },

  onDialogOverlayClosed_() {
    this.disabled = false;
  },

  subtitleText_(locale, manual) {
    const key = manual ? 'manualPasswordTitle' : 'confirmPasswordTitle';
    return this.i18n(key);
  },

  passwordPlaceholder_(locale, manual) {
    const key = manual ? 'manualPasswordInputLabel' : 'confirmPasswordLabel';
    return this.i18n(key);
  },

  passwordErrorText_(locale, manual) {
    const key =
        manual ? 'manualPasswordMismatch' : 'confirmPasswordIncorrectPassword';
    return this.i18n(key);
  },
});
})();
