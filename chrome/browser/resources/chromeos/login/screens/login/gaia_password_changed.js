// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for GAIA password changed screen.
 */

/* #js_imports_placeholder */

/**
 * UI mode for the dialog.
 * @enum {string}
 */
const GaiaPasswordChangedUIState = {
  PASSWORD: 'password',
  FORGOT: 'forgot',
  PROGRESS: 'progress',
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 */
const GaiaPasswordChangedBase = Polymer.mixinBehaviors(
    [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior],
    Polymer.Element);

/**
 * @typedef {{
 *   oldPasswordInput:  CrInputElement,
 * }}
 */
GaiaPasswordChangedBase.$;

class GaiaPasswordChanged extends GaiaPasswordChangedBase {
  static get is() {
    return 'gaia-password-changed-element';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {
      email: String,

      password_: String,

      passwordInvalid_: Boolean,

      disabled: Boolean,
    };
  }

  constructor() {
    super();
    this.email = '';
    this.password_ = '';
    this.passwordInvalid_ = false;
    this.disabled = false;
  }

  defaultUIStep() {
    return GaiaPasswordChangedUIState.PASSWORD;
  }

  get UI_STEPS() {
    return GaiaPasswordChangedUIState;
  }

  /**
   * @override
   */
  ready() {
    super.ready();
    this.initializeLoginScreen('GaiaPasswordChangedScreen');

    cr.ui.LoginUITools.addSubmitListener(
        this.$.oldPasswordInput, this.submit_.bind(this));
  }

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.PASSWORD_CHANGED;
  }

  // Invoked just before being shown. Contains all the data for the screen.
  onBeforeShow(data) {
    this.reset();
    this.email = data && 'email' in data && data.email;
    this.passwordInvalid_ = data && 'showError' in data && data.showError;
  }

  reset() {
    this.setUIStep(GaiaPasswordChangedUIState.PASSWORD);
    this.clearPassword();
    this.disabled = false;
  }

  /**
   * @private
   */
  submit_() {
    if (this.disabled) {
      return;
    }
    if (!this.$.oldPasswordInput.validate()) {
      return;
    }
    this.setUIStep(GaiaPasswordChangedUIState.PROGRESS);
    this.disabled = true;

    chrome.send('migrateUserData', [this.$.oldPasswordInput.value]);
  }

  /** @private */
  onForgotPasswordClicked_() {
    this.setUIStep(GaiaPasswordChangedUIState.FORGOT);
    this.clearPassword();
  }

  /** @private */
  onTryAgainClicked_() {
    this.setUIStep(GaiaPasswordChangedUIState.PASSWORD);
  }

  /** @private */
  onAnimationFinish_() {
    this.focus();
  }

  clearPassword() {
    this.password_ = '';
    this.passwordInvalid_ = false;
  }

  /** @private */
  onProceedClicked_() {
    if (this.disabled) {
      return;
    }
    this.setUIStep(GaiaPasswordChangedUIState.PROGRESS);
    this.disabled = true;
    this.clearPassword();
    this.userActed('resync');
  }

  /** @private */
  onCancel_() {
    if (this.disabled) {
      return;
    }
    this.userActed('cancel');
  }
}

customElements.define(GaiaPasswordChanged.is, GaiaPasswordChanged);
