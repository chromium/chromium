// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import '//resources/ash/common/cr_elements/cr_button/cr_button.js';
import '//resources/ash/common/cr_elements/cr_input/cr_input.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/dialogs/oobe_loading_dialog.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/buttons/oobe_text_button.js';

import {CrInputElement} from '//resources/ash/common/cr_elements/cr_input/cr_input.js';
import {assert} from '//resources/js/assert.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeModalDialog} from '../../components/dialogs/oobe_modal_dialog.js';
import {OobeUiState} from '../../components/display_manager_types.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';
import {addSubmitListener} from '../../login_ui_tools.js';

import {getTemplate} from './saml_confirm_password.html.js';

enum SamlConfirmPasswordState {
  PASSWORD = 'password',
  PROGRESS = 'progress',
}

const SamlConfirmPasswordBase =
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement)));

interface SamlConfirmPasswordScreenData {
  email: string;
  manualPasswordInput: boolean;
}

class SamlConfirmPassword extends SamlConfirmPasswordBase {
  static get is() {
    return 'saml-confirm-password-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
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

  email: string;
  isManualInput: boolean;

  constructor() {
    super();
  }

  override get EXTERNAL_API(): string[] {
    return ['showPasswordStep'];
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep(): string {
    return SamlConfirmPasswordState.PROGRESS;
  }

  override get UI_STEPS() {
    return SamlConfirmPasswordState;
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('ConfirmSamlPasswordScreen');

    addSubmitListener(this.getPasswordInput(), this.submit.bind(this));
    addSubmitListener(this.getConfirmPasswordInput(), this.submit.bind(this));
  }

  /** Initial UI State for screen */
  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState(): OobeUiState {
    return OobeUiState.SAML_PASSWORD_CONFIRM;
  }

  /**
   * Event handler that is invoked just before the screen is shown.
   * @param data Screen init payload
   */
  override onBeforeShow(data: SamlConfirmPasswordScreenData) {
    super.onBeforeShow(data);
    this.reset();
    this.email = data['email'];
    this.isManualInput = data['manualPasswordInput'];
  }

  showPasswordStep(retry: boolean): void {
    if (retry) {
      this.reset();
      this.getPasswordInput().invalid = true;
    }
    this.setUIStep(SamlConfirmPasswordState.PASSWORD);
  }

  private resetFields(): void {
    const passwordInput = this.getPasswordInput();
    passwordInput.invalid = false;
    passwordInput.value = '';
    if (this.isManualInput) {
      const confirmPasswordInput = this.getConfirmPasswordInput();
      confirmPasswordInput.invalid = false;
      confirmPasswordInput.value = '';
    }
  }

  private reset(): void {
    const cancelConfirmDialog = this.getCancelConfirmDialog();
    if (cancelConfirmDialog.open) {
      cancelConfirmDialog.hideDialog();
    }
    this.resetFields();
  }

  private onCancel(): void {
    this.getCancelConfirmDialog().showDialog();
  }

  private onCancelNo(): void {
    this.getCancelConfirmDialog().hideDialog();
  }

  private onCancelYes(): void {
    this.getCancelConfirmDialog().hideDialog();
    this.userActed('cancel');
  }

  private submit(): void {
    const passwordInput = this.getPasswordInput();
    if (!passwordInput.validate()) {
      return;
    }
    if (this.isManualInput) {
      // When using manual password entry, both passwords must match.
      const confirmPasswordInput = this.getConfirmPasswordInput();
      if (!confirmPasswordInput.validate()) {
        return;
      }

      if (confirmPasswordInput.value !== passwordInput.value) {
        passwordInput.invalid = true;
        confirmPasswordInput.invalid = true;
        return;
      }
    }
    this.setUIStep(SamlConfirmPasswordState.PROGRESS);
    this.userActed(['inputPassword', passwordInput.value]);
    this.resetFields();
  }

  private subtitleText(locale: string, manual: boolean): string {
    const key = manual ? 'manualPasswordTitle' : 'confirmPasswordTitle';
    return this.i18nDynamic(locale, key);
  }

  private passwordPlaceholder(locale: string, manual: boolean): string {
    const key = manual ? 'manualPasswordInputLabel' : 'confirmPasswordLabel';
    return this.i18nDynamic(locale, key);
  }

  private passwordErrorText(locale: string, manual: boolean): string {
    const key =
        manual ? 'manualPasswordMismatch' : 'confirmPasswordIncorrectPassword';
    return this.i18nDynamic(locale, key);
  }

  private getPasswordInput(): CrInputElement {
    const passwordInput =
        this.shadowRoot?.querySelector<CrInputElement>('#passwordInput');
    assert(passwordInput instanceof CrInputElement);
    return passwordInput;
  }

  private getConfirmPasswordInput(): CrInputElement {
    const confirmPasswordInput =
        this.shadowRoot?.querySelector<CrInputElement>('#confirmPasswordInput');
    assert(confirmPasswordInput instanceof CrInputElement);
    return confirmPasswordInput;
  }

  private getCancelConfirmDialog(): OobeModalDialog {
    const cancelConfirmDialog =
        this.shadowRoot?.querySelector<OobeModalDialog>('#cancelConfirmDlg');
    assert(cancelConfirmDialog instanceof OobeModalDialog);
    return cancelConfirmDialog;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SamlConfirmPassword.is]: SamlConfirmPassword;
  }
}

customElements.define(SamlConfirmPassword.is, SamlConfirmPassword);
