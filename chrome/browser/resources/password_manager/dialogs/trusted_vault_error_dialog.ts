// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../shared_style.css.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl} from '../password_manager_proxy.js';

import {getTemplate} from './trusted_vault_error_dialog.html.js';

export interface TrustedVaultErrorDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

export class TrustedVaultErrorDialogElement extends PolymerElement {
  static get is() {
    return 'trusted-vault-error-dialog';
  }

  static get template() {
    return getTemplate();
  }

  private onCancelButtonClick_() {
    this.$.dialog.cancel();
  }

  private onVerifyButtonClick_() {
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
