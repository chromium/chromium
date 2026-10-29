// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {PasswordManagerImpl} from '../password_manager_proxy.js';

import {getCss} from './edit_passkey_dialog.css.js';
import {getHtml} from './edit_passkey_dialog.html.js';

export interface EditPasskeyDialogElement {
  $: {
    cancelButton: CrButtonElement,
    dialog: CrDialogElement,
    saveButton: CrButtonElement,
    usernameInput: CrInputElement,
    displayNameInput: CrInputElement,
  };
}

const EditPasskeyDialogElementBase = I18nMixinLit(CrLitElement);

export class EditPasskeyDialogElement extends EditPasskeyDialogElementBase {
  static get is() {
    return 'edit-passkey-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      passkey: {type: Object},
      username_: {type: String},
      displayName_: {type: String},
    };
  }

  accessor passkey: chrome.passwordsPrivate.PasswordUiEntry;
  protected accessor username_: string = '';
  protected accessor displayName_: string = '';

  override connectedCallback() {
    super.connectedCallback();
    assert(this.passkey.isPasskey);

    this.username_ = this.passkey.username;
    this.displayName_ = this.passkey.displayName || '';
  }

  protected onDisplayNameValueChanged_(e: CustomEvent<{value: string}>) {
    this.displayName_ = e.detail.value;
  }

  protected onUsernameValueChanged_(e: CustomEvent<{value: string}>) {
    this.username_ = e.detail.value;
  }

  protected onCancelClick_() {
    this.$.dialog.close();
  }

  protected onEditClick_() {
    this.passkey.username = this.username_;
    this.passkey.displayName = this.displayName_;
    PasswordManagerImpl.getInstance()
        .changeCredential(this.passkey)
        .finally(() => {
          this.$.dialog.close();
        });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'edit-passkey-dialog': EditPasskeyDialogElement;
  }
}

customElements.define(EditPasskeyDialogElement.is, EditPasskeyDialogElement);
