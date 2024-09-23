// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_input/cr_input_style.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../shared_style.css.js';
import './credential_details_card.css.js';
import '../dialogs/edit_passkey_dialog.js';
import '../dialogs/delete_passkey_dialog.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl, PasswordViewPageInteractions} from '../password_manager_proxy.js';
import {UserUtilMixin} from '../user_utils_mixin.js';

import type {CredentialFieldElement} from './credential_field.js';
import {getTemplate} from './passkey_details_card.html.js';

export interface PasskeyDetailsCardElement {
  $: {
    deleteButton: CrButtonElement,
    domainLabel: HTMLElement,
    editButton: CrButtonElement,
    showMore: HTMLAnchorElement,
    usernameValue: CredentialFieldElement,
    displayNameValue: CredentialFieldElement,
    infoLabel: HTMLElement,
  };
}

const PasskeyDetailsCardElementBase = UserUtilMixin(I18nMixin(PolymerElement));

export class PasskeyDetailsCardElement extends PasskeyDetailsCardElementBase {
  static get is() {
    return 'passkey-details-card';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      passkey: Object,
      interactions_: {
        type: Object,
        value: PasswordViewPageInteractions,
      },
      showEditPasskeyDialog_: Boolean,
      showDeletePasskeyDialog_: Boolean,
      infoLabelText_: String,
    };
  }

  static get observers() {
    return [
      'updatePasskeyManagementInfoLabel_(isSyncingPasswords)',
    ];
  }

  passkey: chrome.passwordsPrivate.PasswordUiEntry;
  private showEditPasskeyDialog_: boolean;
  private showDeletePasskeyDialog_: boolean;
  private infoLabelText_: string;

  private getUsernameValue_(): string {
    return !this.passkey.username || this.passkey.username === '' ?
        this.i18n('usernamePlaceholder') :
        this.passkey.username!;
  }

  private getDisplayNameValue_(): string {
    return !this.passkey.displayName || this.passkey.displayName === '' ?
        this.i18n('displayNamePlaceholder') :
        this.passkey.displayName!;
  }

  private onDeleteClick_() {
    this.showDeletePasskeyDialog_ = true;
    PasswordManagerImpl.getInstance().extendAuthValidity();
    PasswordManagerImpl.getInstance().recordPasswordViewInteraction(
        PasswordViewPageInteractions.PASSKEY_DELETE_BUTTON_CLICKED);
  }

  private onDeletePasskeyDialogClosed_() {
    this.showDeletePasskeyDialog_ = false;
    PasswordManagerImpl.getInstance().extendAuthValidity();
  }

  private onEditClicked_() {
    this.showEditPasskeyDialog_ = true;
    PasswordManagerImpl.getInstance().extendAuthValidity();
    PasswordManagerImpl.getInstance().recordPasswordViewInteraction(
        PasswordViewPageInteractions.PASSKEY_EDIT_BUTTON_CLICKED);
  }

  private onEditPasskeyDialogClosed_() {
    this.showEditPasskeyDialog_ = false;
    PasswordManagerImpl.getInstance().extendAuthValidity();
  }

  private getAriaLabelForPasswordCard_(): string {
    return !this.passkey.username ?
        this.i18n('passkeyDetailsCardNoUsernameAriaLabel') :
        this.i18n('passkeyDetailsCardAriaLabel', this.passkey.username);
  }

  private getAriaLabelForEditButton_(): string {
    return !this.passkey.username ?
        this.i18n('passkeyDetailsCardEditButtonNoUsernameAriaLabel') :
        this.i18n(
            'passkeyDetailsCardEditButtonAriaLabel', this.passkey.username);
  }

  private getAriaLabelForDeleteButton_(): string {
    return !this.passkey.username ?
        this.i18n('passkeyDetailsCardDeleteButtonNoUsernameAriaLabel') :
        this.i18n(
            'passkeyDetailsCardDeleteButtonAriaLabel', this.passkey.username);
  }

  private updatePasskeyManagementInfoLabel_() {
    // Google Password Manager passkeys always have their creation time
    // available.
    assert(this.passkey.creationTime !== undefined);

    const date = new Date(this.passkey.creationTime);
    this.infoLabelText_ = this.i18n(
        'passkeyManagementInfoLabel',
        date.toLocaleDateString(/*locales=*/ undefined, {dateStyle: 'short'}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'passkey-details-card': PasskeyDetailsCardElement;
  }
}

customElements.define(PasskeyDetailsCardElement.is, PasskeyDetailsCardElement);
