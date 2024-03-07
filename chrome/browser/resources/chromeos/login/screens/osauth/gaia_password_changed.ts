// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for GAIA password changed screen.
 */

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/iron-media-query/iron-media-query.js';
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
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeI18nMixin, OobeI18nMixinInterface} from '../../components/mixins/oobe_i18n_mixin.js';
import {OobeUiState} from '../../components/display_manager_types.js';
import {addSubmitListener} from '../../login_ui_tools.js';

import {getTemplate} from './gaia_password_changed.html.js';


/**
 * UI mode for the dialog.
 */
enum GaiaPasswordChangedUiState {
  PASSWORD = 'password',
  FORGOT = 'forgot',
  RECOVERY = 'setup-recovery',
  PROGRESS = 'progress',
}

const GaiaPasswordChangedBase = mixinBehaviors(
                                    [
                                      LoginScreenBehavior,
                                      MultiStepBehavior,
                                    ],
                                    OobeI18nMixin(PolymerElement)) as {
  new (): PolymerElement & OobeI18nMixinInterface &
      LoginScreenBehaviorInterface & MultiStepBehaviorInterface,
};


/**
 * Data that is passed to the screen during onBeforeShow.
 */
interface GaiaPasswordChangedScreenData {
  email: string;
  showError: boolean;
}

export class GaiaPasswordChanged extends GaiaPasswordChangedBase {
  static get is() {
    return 'gaia-password-changed-element' as const;
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

  private email: string;
  private password: string;
  private passwordInvalid: boolean;
  private disabled: boolean;
  private passwordInput: CrInputElement;


  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep(): GaiaPasswordChangedUiState {
    return GaiaPasswordChangedUiState.PASSWORD;
  }

  override get UI_STEPS() {
    return GaiaPasswordChangedUiState;
  }

  // clang-format off
  override get EXTERNAL_API() : string[] {
    return [
      'showWrongPasswordError',
      'suggestRecovery',
    ];
  }
  // clang-format on

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('GaiaPasswordChangedScreen');

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
   * Invoked just before being shown. Contains all the data for the screen.
   */
  onBeforeShow(data: GaiaPasswordChangedScreenData): void {
    this.reset();
    this.email = data.email;
    this.passwordInvalid = data.showError;
  }

  private reset(): void {
    this.setUIStep(GaiaPasswordChangedUiState.PASSWORD);
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
    this.setUIStep(GaiaPasswordChangedUiState.PASSWORD);
  }

  /**
   * Called when password was successfully updated
   * and it is possible to set up recovery for the user.
   */
  suggestRecovery(): void {
    this.disabled = false;
    this.setUIStep(GaiaPasswordChangedUiState.RECOVERY);
  }

  /**
   * Returns the subtitle message for the data loss warning screen.
   * @param locale The i18n locale.
   * @param email The email address that the user is trying to recover.
   * @return The translated subtitle message.
   */
  private getDataLossWarningSubtitleMessage(locale: string, email: string):
      TrustedHTML {
    return this.i18nAdvancedDynamic(
        locale, 'dataLossWarningSubtitle', {substitutions: [email]});
  }

  private submit(): void {
    if (this.disabled) {
      return;
    }
    if (!this.passwordInput.validate()) {
      return;
    }
    this.setUIStep(GaiaPasswordChangedUiState.PROGRESS);
    this.disabled = true;
    this.userActed(['migrate-user-data', this.passwordInput.value]);
  }

  private onForgotPasswordClicked(): void {
    if (this.disabled) {
      return;
    }
    this.setUIStep(GaiaPasswordChangedUiState.FORGOT);
    this.clearPassword();
  }

  private onBackButtonClicked(): void {
    this.setUIStep(GaiaPasswordChangedUiState.PASSWORD);
  }

  private onAnimationFinish(): void {
    this.focus();
  }

  private clearPassword(): void {
    this.password = '';
    this.passwordInvalid = false;
  }

  private onProceedClicked(): void {
    if (this.disabled) {
      return;
    }
    this.setUIStep(GaiaPasswordChangedUiState.PROGRESS);
    this.disabled = true;
    this.clearPassword();
    this.userActed('resync');
  }

  private onNoRecovery(): void {
    if (this.disabled) {
      return;
    }
    this.setUIStep(GaiaPasswordChangedUiState.PROGRESS);
    this.disabled = true;
    this.clearPassword();
    this.userActed('no-recovery');
  }

  private onSetRecovery(): void {
    if (this.disabled) {
      return;
    }
    this.setUIStep(GaiaPasswordChangedUiState.PROGRESS);
    this.disabled = true;
    this.clearPassword();
    this.userActed('setup-recovery');
  }

  private onCancel(): void {
    if (this.disabled) {
      return;
    }
    this.disabled = true;
    this.userActed('cancel');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [GaiaPasswordChanged.is]: GaiaPasswordChanged;
  }
}

customElements.define(GaiaPasswordChanged.is, GaiaPasswordChanged);
