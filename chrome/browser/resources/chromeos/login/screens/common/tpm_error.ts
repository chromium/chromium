// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for TPM error screen.
 */

import '//resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './tpm_error.html.js';


/**
 * UI state for the dialog.
 */
enum TpmUiState {
  DEFAULT = 'default',
  TPM_OWNED = 'tpm-owned',
  DBUS_ERROR = 'dbus-error',
}



export const TPMErrorMessageElementBase =
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement)));

// eslint-disable-next-line @typescript-eslint/naming-convention
export class TPMErrorMessage extends TPMErrorMessageElementBase {
  static get is() {
    return 'tpm-error-message-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {};
  }

  constructor() {
    super();
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('TPMErrorMessageScreen');
  }

  override get EXTERNAL_API(): string[] {
    return [
      'setStep',
    ];
  }

  override get UI_STEPS() {
    return TpmUiState;
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep(): TpmUiState {
    return TpmUiState.DEFAULT;
  }


  setStep(step: string): void {
    this.setUIStep(step);
  }

  private onRestartClicked(): void {
    this.userActed('reboot-system');
  }


  override get defaultControl(): HTMLElement|null {
    return this.shadowRoot!.querySelector('#errorDialog');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [TPMErrorMessage.is]: TPMErrorMessage;
  }
}

customElements.define(TPMErrorMessage.is, TPMErrorMessage);
