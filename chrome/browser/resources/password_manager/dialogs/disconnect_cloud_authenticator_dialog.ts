// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import '../shared_style.css.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl} from '../password_manager_proxy.js';

import {getTemplate} from './disconnect_cloud_authenticator_dialog.html.js';

export interface DisconnectCloudAuthenticatorDialogElement {
  $: {
    cancelButton: CrButtonElement,
    confirmButton: CrButtonElement,
    dialog: CrDialogElement,
  };
}

const DisconnectCloudAuthenticatorDialogElementBase = I18nMixin(PolymerElement);

export class DisconnectCloudAuthenticatorDialogElement extends
    DisconnectCloudAuthenticatorDialogElementBase {
  static get is() {
    return 'disconnect-cloud-authenticator-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {};
  }

  override ready() {
    super.ready();
  }

  private onCancel_(): void {
    this.$.dialog.close();
  }

  private onConfirm_(): void {
    PasswordManagerImpl.getInstance().disconnectCloudAuthenticator().then(
        this.onDisconnectCloudAuthenticatorResponse_.bind(this));
    this.$.dialog.close();
  }

  private onDisconnectCloudAuthenticatorResponse_(success: boolean): void {
    this.dispatchEvent(new CustomEvent('disconnect-cloud-authenticator', {
      bubbles: true,
      composed: true,
      detail: {success: success},
    }));
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
