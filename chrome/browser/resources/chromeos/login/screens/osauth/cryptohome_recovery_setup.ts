// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/dialogs/oobe_loading_dialog.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './cryptohome_recovery_setup.html.js';

/**
 * UI mode for the dialog.
 */
// eslint-disable-next-line @typescript-eslint/naming-convention
enum CryptohomeRecoverySetupUIState {
  LOADING = 'loading',
  ERROR = 'error',
}

const CryptohomeRecoverySetupBase =
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement)));

class CryptohomeRecoverySetup extends CryptohomeRecoverySetupBase {
  static get is() {
    return 'cryptohome-recovery-setup-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {};
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep(): string {
    return CryptohomeRecoverySetupUIState.LOADING;
  }

  override get UI_STEPS() {
    return CryptohomeRecoverySetupUIState;
  }

  override get EXTERNAL_API(): string[] {
    return [
      'setLoadingState',
      'onSetupFailed',
    ];
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('CryptohomeRecoverySetupScreen');
  }

  reset(): void {
    this.setUIStep(CryptohomeRecoverySetupUIState.LOADING);
  }

  /**
   * Called to show the spinner in the UI.
   */
  setLoadingState(): void {
    this.setUIStep(CryptohomeRecoverySetupUIState.LOADING);
  }

  /**
   * Called when Cryptohome recovery setup failed.
   */
  onSetupFailed(): void {
    this.setUIStep(CryptohomeRecoverySetupUIState.ERROR);
  }

  /**
   * Skip button click handler.
   */
  private onSkip(): void {
    this.userActed('skip');
  }

  /**
   * Retry button click handler.
   */
  private onRetry(): void {
    this.userActed('retry');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [CryptohomeRecoverySetup.is]: CryptohomeRecoverySetup;
  }
}

customElements.define(CryptohomeRecoverySetup.is, CryptohomeRecoverySetup);
