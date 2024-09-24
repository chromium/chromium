// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/dialogs/oobe_loading_dialog.js';
import '../../components/buttons/oobe_back_button.js';
import '../../components/buttons/oobe_next_button.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './account_selection.html.js';

/**
 * Choices a user can make.
 */
enum AccountSelectionOptions {
  REUSE_ACCOUNT = 'reuseAccountFromEnrollment',
  SIGNIN_AGAIN = 'signinAgain',
}

/**
 * UI mode for the dialog.
 */
enum AccountSelectionState {
  SELECTION = 'selection',
  PROGRESS = 'progress',
}

const AccountSelectionBase = OobeDialogHostMixin(
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement))));

export class AccountSelection extends AccountSelectionBase {
  static get is() {
    return 'account-selection-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * Option chosen during account selection stpe.
       */
      selectedAccountOption: {
        type: String,
      },

      /**
       * Options for the account selections step.
       */
      accountSelectionEnum: {
        readOnly: true,
        type: Object,
        value: AccountSelectionOptions,
      },

      /**
       * Email used during enrollment, used for the account selection step.
       */
      enrollmentEmail: {
        type: String,
      },
    };
  }

  private enrollmentEmail: string;
  private selectedAccountOption: string;
  private accountSelectionEnum: AccountSelectionOptions;

  override ready(): void {
    super.ready();

    this.initializeLoginScreen('AccountSelectionScreen');
  }
  override get EXTERNAL_API(): string[] {
    return [
      'showStepProgress',
      'setUserEmail',
    ];
  }

  override get UI_STEPS() {
    return AccountSelectionState;
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep(): AccountSelectionState {
    return AccountSelectionState.SELECTION;
  }

  // Invoked just before being shown. Contains all the data for the screen.
  override onBeforeShow(): void {
    super.onBeforeShow();
    this.selectedAccountOption = AccountSelectionOptions.REUSE_ACCOUNT;
  }

  showStepProgress(): void {
    this.setUIStep(AccountSelectionState.PROGRESS);
  }

  setUserEmail(emailAddress: string): void {
    this.enrollmentEmail = emailAddress;
  }

  private onNextClicked(): void {
    this.userActed(this.selectedAccountOption);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [AccountSelection.is]: AccountSelection;
  }
}

customElements.define(AccountSelection.is, AccountSelection);
