// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './shared_style.css.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './password_details_card.html.js';

export interface PasswordDetailsCardElement {
  $: {
    copyPasswordButton: CrIconButtonElement,
    copyUsernameButton: CrIconButtonElement,
    deleteButton: CrButtonElement,
    editButton: CrButtonElement,
    linkValue: HTMLElement,
    passwordValue: CrInputElement,
    showPasswordButton: CrIconButtonElement,
    usernameValue: CrInputElement,
  };
}


export class PasswordDetailsCardElement extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'password-details-card';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      password: Object,

      isPasswordVisible_: {
        type: Boolean,
        value: false,
      },
    };
  }

  password: chrome.passwordsPrivate.PasswordUiEntry;
  private isPasswordVisible_: boolean;

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
    return this.isFederated_() || this.isPasswordVisible_ ? 'text' : 'password';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'password-details-card': PasswordDetailsCardElement;
  }
}

customElements.define(
    PasswordDetailsCardElement.is, PasswordDetailsCardElement);
