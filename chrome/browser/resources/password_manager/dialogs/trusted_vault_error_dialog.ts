// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {PasswordManagerImpl} from '../password_manager_proxy.js';

import {getCss} from './trusted_vault_error_dialog.css.js';
import {getHtml} from './trusted_vault_error_dialog.html.js';

export interface TrustedVaultErrorDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

export class TrustedVaultErrorDialogElement extends CrLitElement {
  static get is() {
    return 'trusted-vault-error-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  protected onCancelButtonClick_() {
    this.$.dialog.cancel();
  }

  protected onVerifyButtonClick_() {
    PasswordManagerImpl.getInstance().startTrustedVaultUnlock();
    this.$.dialog.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'trusted-vault-error-dialog': TrustedVaultErrorDialogElement;
  }
}

customElements.define(
    TrustedVaultErrorDialogElement.is, TrustedVaultErrorDialogElement);
