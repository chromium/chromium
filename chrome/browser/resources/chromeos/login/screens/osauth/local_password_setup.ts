// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/auth_setup/set_local_password_input.js';
import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import '//resources/ash/common/cr_elements/cr_button/cr_button.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/dialogs/oobe_loading_dialog.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/buttons/oobe_back_button.js';

import {SetLocalPasswordInputElement} from '//resources/ash/common/auth_setup/set_local_password_input.js';
import {assert} from '//resources/js/assert.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeUiState} from '../../components/display_manager_types.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './local_password_setup.html.js';


/**
 * UI mode for the dialog.
 */
enum LocalPasswordSetupState {
  PASSWORD = 'password',
  PROGRESS = 'progress',
}

const LocalPasswordSetupBase = OobeDialogHostMixin(
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement))));

/**
 * Data that is passed to the screen during onBeforeShow.
 */
interface LocalPasswordSetupScreenData {
  showBackButton: boolean;
  isRecoveryFlow: boolean;
}

export class LocalPasswordSetup extends LocalPasswordSetupBase {
  static get is() {
    return 'local-password-setup-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       */
      backButtonVisible: {
        type: Boolean,
      },

      isRecoveryFlow: {
        type: Boolean,
      },

      passwordValue: {
        type: String,
        value: null,
      },
    };
  }

  private backButtonVisible: boolean;
  private isRecoveryFlow: boolean;
  private passwordValue: string;

  constructor() {
    super();
    this.backButtonVisible = true;
  }

  override get EXTERNAL_API(): string[] {
    return ['showLocalPasswordSetupFailure'];
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep(): LocalPasswordSetupState {
    return LocalPasswordSetupState.PASSWORD;
  }

  override get UI_STEPS() {
    return LocalPasswordSetupState;
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('LocalPasswordSetupScreen');
  }

  /** Initial UI State for screen */
  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState(): OobeUiState {
    return OobeUiState.ONBOARDING;
  }

  /**
   * Event handler that is invoked just before the screen is shown.
   * @param data Screen initial payload
   */
  override onBeforeShow(data: LocalPasswordSetupScreenData): void {
    super.onBeforeShow(data);
    this.reset();
    this.backButtonVisible = data['showBackButton'];
    this.isRecoveryFlow = data['isRecoveryFlow'];
  }

  showLocalPasswordSetupFailure(): void {
    // TODO(b/304963851): Show setup failed message, likely allowing user to
    // retry.
  }

  private reset(): void {
    const passwordInput = this.shadowRoot?.querySelector('#passwordInput');
    if (passwordInput instanceof SetLocalPasswordInputElement) {
      passwordInput.reset();
    }
  }

  private getPasswordInput(): SetLocalPasswordInputElement {
    const passwordInput = this.shadowRoot?.querySelector('#passwordInput');
    assert(passwordInput instanceof SetLocalPasswordInputElement);
    return passwordInput;
  }

  private onBackClicked(): void {
    if (!this.backButtonVisible) {
      return;
    }

    this.userActed([
      'back',
      this.getPasswordInput().value,
    ]);
  }

  async onSubmit(): Promise<void> {
    const passwordInput = this.getPasswordInput();
    await passwordInput.validate();

    if (passwordInput.value === null) {
      return;
    }

    this.setUIStep(LocalPasswordSetupState.PROGRESS);
    this.userActed([
      'inputPassword',
      passwordInput.value,
    ]);
  }

  private onDoneClicked(): void {
    this.userActed(['done']);
  }

  private titleText(_locale: string, isRecoveryFlow: boolean) {
    const key =
        isRecoveryFlow ? 'localPasswordResetTitle' : 'localPasswordSetupTitle';
    return this.i18n(key);
  }

  private isValid(password: string) {
    return !!password;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [LocalPasswordSetup.is]: LocalPasswordSetup;
  }
}

customElements.define(LocalPasswordSetup.is, LocalPasswordSetup);
