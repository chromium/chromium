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
  }

  get EXTERNAL_API() {
    return ['retry'];
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
   * Event handler that is invoked just before the screen is shown.
   * @param {Object} data Screen init payload
   */
  onBeforeShow(data) {
    this.reset_();
    this.email = data['email'];
    this.isManualInput = data['manualPasswordInput'];
  }

  retry() {
    this.reset_();
    this.$.passwordInput.invalid = true;
  }

  resetFields_() {
    this.$.passwordInput.invalid = false;
    this.$.passwordInput.value = '';
    if (this.isManualInput) {
      this.shadowRoot.querySelector('#confirmPasswordInput').invalid = false;
      this.shadowRoot.querySelector('#confirmPasswordInput').value = '';
    }
  }

  reset_() {
    if (this.$.cancelConfirmDlg.open)
      this.$.cancelConfirmDlg.hideDialog();
    this.setUIStep(SamlConfirmPasswordState.PASSWORD);
    this.resetFields_();
  }


  onCancel_() {
    this.$.cancelConfirmDlg.showDialog();
  }

  onCancelNo_() {
    this.$.cancelConfirmDlg.hideDialog();
  }

  onCancelYes_() {
    this.$.cancelConfirmDlg.hideDialog();
    this.userActed('cancel');
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
    this.userActed(['inputPassword', this.$.passwordInput.value]);
    this.resetFields_();
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
