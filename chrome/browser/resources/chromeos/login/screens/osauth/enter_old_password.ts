// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/ash/common/cr_elements/cr_input/cr_input.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/dialogs/oobe_loading_dialog.js';
import '../../components/buttons/oobe_text_button.js';

import {CrInputElement} from '//resources/ash/common/cr_elements/cr_input/cr_input.js';
import {assert} from '//resources/js/assert.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeUiState} from '../../components/display_manager_types.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';
import {addSubmitListener} from '../../login_ui_tools.js';

import {getTemplate} from './enter_old_password.html.js';


/**
 * UI mode for the dialog.
 */
enum EnterOldPasswordUiState {
  PASSWORD = 'password',
  PROGRESS = 'progress',
}


const EnterOldPasswordBase =
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement)));


export class EnterOldPassword extends EnterOldPasswordBase {
  static get is() {
    return 'enter-old-password-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      password: {
        type: String,
        value: '',
      },

      passwordInvalid: {
        type: Boolean,
        value: false,
      },

      disabled: {
        type: Boolean,
        value: false,
      },

      passwordInput: Object,
    };
  }

  private password: string;
  private passwordInvalid: boolean;
  private disabled: boolean;
  private passwordInput: CrInputElement;

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep(): EnterOldPasswordUiState {
    return EnterOldPasswordUiState.PASSWORD;
  }

  override get UI_STEPS() {
    return EnterOldPasswordUiState;
  }

  /** Overridden from LoginScreenBehavior. */
  override get EXTERNAL_API(): string[] {
    return [
      'showWrongPasswordError',
    ];
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('EnterOldPasswordScreen');

    const oldpasswordInput =
        this.shadowRoot?.querySelector<CrInputElement>('#oldPasswordInput');
    assert(oldpasswordInput instanceof CrInputElement);
    this.passwordInput = oldpasswordInput;
    addSubmitListener(this.passwordInput, this.submit.bind(this));
  }

  /** Initial UI State for screen */
  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState(): OobeUiState {
    return OobeUiState.PASSWORD_CHANGED;
  }

  /**
   * Invoked just before being shown.
   */
  override onBeforeShow(): void {
    super.onBeforeShow();
    this.reset();
  }

  private reset(): void {
    this.setUIStep(EnterOldPasswordUiState.PASSWORD);
    this.clearPassword();
    this.disabled = false;
  }

  /**
   * Called when Screen fails to authenticate with
   * provided password.
   */
  showWrongPasswordError(): void {
    this.clearPassword();
    this.disabled = false;
    this.passwordInvalid = true;
    this.setUIStep(EnterOldPasswordUiState.PASSWORD);
  }

  private submit(): void {
    if (this.disabled) {
      return;
    }
    if (!this.passwordInput.validate()) {
      return;
    }
    this.setUIStep(EnterOldPasswordUiState.PROGRESS);
    this.disabled = true;
    this.userActed(['submit', this.passwordInput.value]);
  }

  private onForgotPasswordClicked(): void {
    if (this.disabled) {
      return;
    }
    this.userActed('forgot');
  }

  private onAnimationFinish(): void {
    this.focus();
  }

  private clearPassword(): void {
    this.password = '';
    this.passwordInvalid = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [EnterOldPassword.is]: EnterOldPassword;
  }
}

customElements.define(EnterOldPassword.is, EnterOldPassword);
