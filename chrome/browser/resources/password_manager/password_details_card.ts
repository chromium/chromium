// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import './shared_style.css.js';

import {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './password_details_card.html.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';
import {ShowPasswordMixin} from './show_password_mixin.js';

export interface PasswordDetailsCardElement {
  $: {
    copyPasswordButton: CrIconButtonElement,
    copyUsernameButton: CrIconButtonElement,
    deleteButton: CrButtonElement,
    editButton: CrButtonElement,
    passwordValue: CrInputElement,
    showPasswordButton: CrIconButtonElement,
    toast: CrToastElement,
    usernameValue: CrInputElement,
  };
}

const PasswordDetailsCardElementBase =
    ShowPasswordMixin(I18nMixin(PolymerElement));

export class PasswordDetailsCardElement extends PasswordDetailsCardElementBase {
  static get is() {
    return 'password-details-card';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      password: Object,
      toastMessage_: String,
    };
  }

  password: chrome.passwordsPrivate.PasswordUiEntry;
  private toastMessage_: string;

  private isFederated_(): boolean {
    return !!this.password.federationText;
  }

  private getPasswordLabel_() {
    return this.isFederated_() ? this.i18n('federationLabel') :
                                 this.i18n('passwordLabel');
  }

  private getPasswordValue_(): string|undefined {
    return this.isFederated_() ? this.password.federationText :
                                 this.password.password;
  }

  private getPasswordType_(): string {
    return this.isFederated_() ? 'text' : this.getPasswordInputType();
  }

  private onCopyPasswordClick_() {
    PasswordManagerImpl.getInstance()
        .requestPlaintextPassword(
            this.password.id, chrome.passwordsPrivate.PlaintextReason.COPY)
        .then(() => this.showToast_(this.i18n('passwordCopiedToClipboard')))
        .catch(() => {});
  }

  private onCopyUsernameClick_() {
    navigator.clipboard.writeText(this.password.username);
    this.showToast_(this.i18n('usernameCopiedToClipboard'));
  }

  private showToast_(message: string) {
    this.toastMessage_ = message;
    this.$.toast.show();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'password-details-card': PasswordDetailsCardElement;
  }
}

customElements.define(
    PasswordDetailsCardElement.is, PasswordDetailsCardElement);
