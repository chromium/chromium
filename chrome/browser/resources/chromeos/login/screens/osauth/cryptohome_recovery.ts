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
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';

import {getTemplate} from './cryptohome_recovery.html.js';

// eslint-disable-next-line @typescript-eslint/naming-convention
enum CryptohomeRecoveryUIState {
  LOADING = 'loading',
  DONE = 'done',
  ERROR = 'error',
  REAUTH_NOTIFICATION = 'reauth-notification',
}

const CryptohomeRecoveryBase =
    mixinBehaviors(
      [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior],
      PolymerElement) as {
        new (): PolymerElement & OobeI18nBehaviorInterface &
            LoginScreenBehaviorInterface & MultiStepBehaviorInterface,
  };

class CryptohomeRecovery extends CryptohomeRecoveryBase {
  static get is() {
    return 'cryptohome-recovery-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * Whether the page is being rendered in dark mode.
       */
      isDarkModeActive: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the buttons on the screen are disabled. Prevents sending double
       * requests.
       */
      disabled: {
        type: Boolean,
        value: false,
      },
    };
  }

  private isDarkModeActive: boolean;
  private disabled: boolean;

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep(): string {
    return CryptohomeRecoveryUIState.LOADING;
  }

  override get UI_STEPS() {
    return CryptohomeRecoveryUIState;
  }

  override get EXTERNAL_API(): string[] {
    return [
      'onRecoverySucceeded',
      'onRecoveryFailed',
      'showReauthNotification',
    ];
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('CryptohomeRecoveryScreen');
  }

  /**
   * Invoked just before being shown.
   */
  onBeforeShow(): void {
    this.reset();
  }

  reset(): void {
    this.setUIStep(CryptohomeRecoveryUIState.LOADING);
    this.disabled = false;
  }

  /**
   * Called when Cryptohome recovery succeeded.
   */
  onRecoverySucceeded(): void {
    this.setUIStep(CryptohomeRecoveryUIState.DONE);
    this.disabled = false;
  }

  /**
   * Called when Cryptohome recovery failed.
   */
  onRecoveryFailed(): void {
    this.setUIStep(CryptohomeRecoveryUIState.ERROR);
    this.disabled = false;
  }

  /**
   * Shows a reauth required message when there's no reauth proof token.
   */
  showReauthNotification(): void {
    this.setUIStep(CryptohomeRecoveryUIState.REAUTH_NOTIFICATION);
    this.disabled = false;
  }

  /**
   * Enter old password button click handler.
   */
  private onGoToManualRecovery(): void {
    if (this.disabled) {
      return;
    }
    this.disabled = true;
    this.userActed('enter-old-password');
  }

  /**
   * Retry button click handler.
   */
  private onRetry(): void {
    if (this.disabled) {
      return;
    }
    this.disabled = true;
    this.userActed('retry');
  }

  /**
   * Done button click handler.
   */
  private onDone(): void {
    if (this.disabled) {
      return;
    }
    this.disabled = true;
    this.userActed('done');
  }

  /**
   * Click handler for the next button on the reauth notification screen.
   */
  private onReauthButtonClicked(): void {
    if (this.disabled) {
      return;
    }
    this.disabled = true;
    this.userActed('reauth');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [CryptohomeRecovery.is]: CryptohomeRecovery;
  }
}

customElements.define(CryptohomeRecovery.is, CryptohomeRecovery);
