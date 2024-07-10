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

import {getTemplate} from './cryptohome_recovery.html.js';

// eslint-disable-next-line @typescript-eslint/naming-convention
enum CryptohomeRecoveryUIState {
  LOADING = 'loading',
  REAUTH_NOTIFICATION = 'reauth-notification',
}

const CryptohomeRecoveryBase =
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement)));

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
  override onBeforeShow(): void {
    super.onBeforeShow();
    this.reset();
  }

  reset(): void {
    this.setUIStep(CryptohomeRecoveryUIState.LOADING);
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
