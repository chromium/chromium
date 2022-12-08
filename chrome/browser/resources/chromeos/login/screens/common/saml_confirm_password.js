// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.m.js';
import '../../components/common_styles/oobe_common_styles.m.js';
import '../../components/common_styles/oobe_dialog_host_styles.m.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/dialogs/oobe_loading_dialog.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/buttons/oobe_text_button.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.m.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.m.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OobeModalDialog} from '../../components/dialogs/oobe_modal_dialog.js';
import {OOBE_UI_STATE} from '../../components/display_manager_types.js';
import {addSubmitListener} from '../../login_ui_tools.js';


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
const SamlConfirmPasswordBase = mixinBehaviors(
    [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior], PolymerElement);

/**
 * @typedef {{
 *   passwordInput:  CrInputElement,
 *   confirmPasswordInput: CrInputElement,
 *   cancelConfirmDlg: OobeModalDialog
 * }}
 */
SamlConfirmPasswordBase.$;

/**
 * @polymer
 */
class SamlConfirmPassword extends SamlConfirmPasswordBase {
  static get is() {
    return 'saml-confirm-password-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      email: {
        type: String,
        value: '',
      },

      isManualInput: {
        type: Boolean,
        value: false,
      },
    };
  }

  constructor() {
    super();
  }

  get EXTERNAL_API() {
    return ['showPasswordStep'];
  }

  defaultUIStep() {
    return SamlConfirmPasswordState.PROGRESS;
  }

  get UI_STEPS() {
    return SamlConfirmPasswordState;
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('ConfirmSamlPasswordScreen');

    addSubmitListener(this.$.passwordInput, this.submit_.bind(this));
    addSubmitListener(this.$.confirmPasswordInput, this.submit_.bind(this));
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

  showPasswordStep(retry) {
    if (retry) {
      this.reset_();
      this.$.passwordInput.invalid = true;
    }
    this.setUIStep(SamlConfirmPasswordState.PASSWORD);
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
    if (this.$.cancelConfirmDlg.open) {
      this.$.cancelConfirmDlg.hideDialog();
    }
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
    if (!this.$.passwordInput.validate()) {
      return;
    }
    if (this.isManualInput) {
      // When using manual password entry, both passwords must match.
      var confirmPasswordInput = this.shadowRoot.querySelector('#confirmPasswordInput');
      if (!confirmPasswordInput.validate()) {
        return;
      }

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
