// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {PasswordManagerImpl} from '../password_manager_proxy.js';
import {TrustedVaultUnlockDialogSource} from '../user_utils_mixin_lit.js';

import {getCss} from './trusted_vault_error_dialog.css.js';
import {getHtml} from './trusted_vault_error_dialog.html.js';

/**
 * User interactions with the trusted vault unlock dialog. These values are
 * persisted to logs. Entries should not be renumbered and numeric values should
 * never be reused.
 */
// LINT.IfChange(TrustedVaultUnlockDialogAction)
export enum TrustedVaultUnlockDialogAction {
  // The dialog was shown.
  SHOWN = 0,
  // The dialog was dismissed, e.g. via the "Not now" button or the Esc key.
  DISMISSED = 1,
  // The user clicked the button which starts the unlock flow.
  CONTINUE = 2,
  // Must be last.
  COUNT = 3,
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/password/enums.xml:TrustedVaultUnlockDialogAction)

function recordTrustedVaultUnlockDialogAction(
    source: TrustedVaultUnlockDialogSource,
    action: TrustedVaultUnlockDialogAction) {
  chrome.metricsPrivate.recordEnumerationValue(
      `PasswordManager.TrustedVaultUnlockDialog.Action.${source}`, action,
      TrustedVaultUnlockDialogAction.COUNT);
}

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

  static override get properties() {
    return {
      /** What caused the dialog to be shown. Used for metrics. */
      source: {type: String},
    };
  }

  accessor source: TrustedVaultUnlockDialogSource =
      TrustedVaultUnlockDialogSource.AUTO_ON_LOAD;

  override connectedCallback() {
    super.connectedCallback();
    // The dialog is shown as soon as it is attached (`show-on-attach`).
    recordTrustedVaultUnlockDialogAction(
        this.source, TrustedVaultUnlockDialogAction.SHOWN);
  }

  protected onCancelButtonClick_() {
    this.$.dialog.cancel();
  }

  // Called when the dialog is canceled: via the "Not now" button, the Esc key
  // or a back navigation.
  protected onDialogCancel_() {
    recordTrustedVaultUnlockDialogAction(
        this.source, TrustedVaultUnlockDialogAction.DISMISSED);
  }

  protected onVerifyButtonClick_() {
    recordTrustedVaultUnlockDialogAction(
        this.source, TrustedVaultUnlockDialogAction.CONTINUE);
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
