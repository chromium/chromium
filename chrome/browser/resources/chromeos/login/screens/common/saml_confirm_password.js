// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* #js_imports_placeholder */

/**
 * UI mode for the dialog.
 * @enum {string}
 */
const SamlConfirmPasswordState = {
  PASSWORD: 'password',
  PROGRESS: 'progress',
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
 const SamlConfirmPasswordBase = Polymer.mixinBehaviors(
  [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior],
  Polymer.Element);

/**
 * @typedef {{
 *   passwordInput:  CrInputElement,
 *   confirmPasswordInput: CrInputElement,
 *   cancelConfirmDlg: OobeModalDialogElement
 * }}
 */
SamlConfirmPasswordBase.$;

/**
 * @polymer
 */
class SamlConfirmPassword extends SamlConfirmPasswordBase {

  static get is() { return 'saml-confirm-password-element'; }

  /* #html_template_placeholder */

  static get properties() {
    return {
      email: {
        type: String,
        value: '',
      },

      isManualInput: {
        type: Boolean,
        value: false,
      }
    };
  }

  constructor() {
    super();
    /**
     * Callback to run when the screen is dismissed.
     * @type {?function(string)}
     */
    this.callback_ = null;
  }

  get EXTERNAL_API() {
    return ['show'];
  }

  defaultUIStep() {
    return SamlConfirmPasswordState.PASSWORD;
  }

  get UI_STEPS() {
    return SamlConfirmPasswordState;
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('ConfirmSamlPasswordScreen', {
      resetAllowed: true,
    });

    cr.ui.LoginUITools.addSubmitListener(
        this.$.passwordInput, this.submit_.bind(this));
    cr.ui.LoginUITools.addSubmitListener(
        this.$.confirmPasswordInput, this.submit_.bind(this));
  }

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.SAML_PASSWORD_CONFIRM;
  }

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
    Oobe.getInstance().showScreen({id: 'saml-confirm-password'});
  }

  resetFields() {
    this.$.passwordInput.invalid = false;
    this.$.passwordInput.value = '';
    if (this.isManualInput) {
      this.shadowRoot.querySelector('#confirmPasswordInput').invalid = false;
      this.shadowRoot.querySelector('#confirmPasswordInput').value = '';
    }
  }

  reset() {
    if (this.$.cancelConfirmDlg.open)
      this.$.cancelConfirmDlg.hideDialog();
    this.setUIStep(SamlConfirmPasswordState.PASSWORD);
    this.resetFields();
  }


  onCancel_() {
    this.$.cancelConfirmDlg.showDialog();
  }

  onCancelNo_() {
    this.$.cancelConfirmDlg.hideDialog();
  }

  onCancelYes_() {
    this.$.cancelConfirmDlg.hideDialog();

    Oobe.getInstance().showScreen({id: 'gaia-signin'});
  }

  submit_() {
    if (!this.$.passwordInput.validate())
      return;
    if (this.isManualInput) {
      // When using manual password entry, both passwords must match.
      var confirmPasswordInput = this.shadowRoot.querySelector('#confirmPasswordInput');
      if (!confirmPasswordInput.validate())
        return;

      if (confirmPasswordInput.value != this.$.passwordInput.value) {
        this.$.passwordInput.invalid = true;
        confirmPasswordInput.invalid = true;
        return;
      }
    }
    this.setUIStep(SamlConfirmPasswordState.PROGRESS);
    this.callback_(this.$.passwordInput.value);
    this.resetFields();
  }

  subtitleText_(locale, manual) {
    const key = manual ? 'manualPasswordTitle' : 'confirmPasswordTitle';
    return this.i18n(key);
  }

  passwordPlaceholder_(locale, manual) {
    const key = manual ? 'manualPasswordInputLabel' : 'confirmPasswordLabel';
    return this.i18n(key);
  }

  passwordErrorText_(locale, manual) {
    const key =
        manual ? 'manualPasswordMismatch' : 'confirmPasswordIncorrectPassword';
    return this.i18n(key);
  }
}

customElements.define(SamlConfirmPassword.is, SamlConfirmPassword);
