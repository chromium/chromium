// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {PasswordManagerImpl} from '../password_manager_proxy.js';

import {getHtml} from './disconnect_cloud_authenticator_dialog.html.js';

export interface DisconnectCloudAuthenticatorDialogElement {
  $: {
    cancelButton: CrButtonElement,
    confirmButton: CrButtonElement,
    dialog: CrDialogElement,
  };
}

const DisconnectCloudAuthenticatorDialogElementBase =
    I18nMixinLit(CrLitElement);

export class DisconnectCloudAuthenticatorDialogElement extends
    DisconnectCloudAuthenticatorDialogElementBase {
  static get is() {
    return 'disconnect-cloud-authenticator-dialog';
  }

  override render() {
    return getHtml.bind(this)();
  }

  protected onCancelClick_(): void {
    this.$.dialog.close();
  }

  protected onConfirmClick_(): void {
    PasswordManagerImpl.getInstance().disconnectCloudAuthenticator().then(
        this.onDisconnectCloudAuthenticatorResponse_.bind(this));
    this.$.dialog.close();
  }

  private onDisconnectCloudAuthenticatorResponse_(success: boolean): void {
    this.fire('disconnect-cloud-authenticator', {success});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'disconnect-cloud-authenticator-dialog':
        DisconnectCloudAuthenticatorDialogElement;
  }
}

customElements.define(
    DisconnectCloudAuthenticatorDialogElement.is,
    DisconnectCloudAuthenticatorDialogElement);
