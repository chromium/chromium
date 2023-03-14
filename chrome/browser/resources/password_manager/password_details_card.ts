// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_input/cr_input_style.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import './shared_style.css.js';
import './dialogs/edit_password_dialog.js';

import {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './password_details_card.html.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';
import {ShowPasswordMixin} from './show_password_mixin.js';

export type PasswordRemovedEvent =
    CustomEvent<{removedFromStores: chrome.passwordsPrivate.PasswordStoreSet}>;

declare global {
  interface HTMLElementEventMap {
    'password-removed': PasswordRemovedEvent;
  }
}

export interface PasswordDetailsCardElement {
  $: {
    copyPasswordButton: CrIconButtonElement,
    copyUsernameButton: CrIconButtonElement,
    deleteButton: CrButtonElement,
    editButton: CrButtonElement,
    noteValue: HTMLElement,
    passwordValue: CrInputElement,
    showMore: HTMLAnchorElement,
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

      showNoteFully_: Boolean,

      showEditPasswordDialog_: Boolean,
    };
  }

  password: chrome.passwordsPrivate.PasswordUiEntry;
  private toastMessage_: string;
  private noteRows_: number;
  private showNoteFully_: boolean;
  private showEditPasswordDialog_: boolean;

  override connectedCallback() {
    super.connectedCallback();
    if (this.isFederated_()) {
      return;
    }
    // Set default value here so listeners can be updated properly.
    this.showNoteFully_ = false;
  }

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
    this.extendAuthValidity_();
  }

  private onCopyUsernameClick_() {
    navigator.clipboard.writeText(this.password.username);
    this.showToast_(this.i18n('usernameCopiedToClipboard'));
    this.extendAuthValidity_();
  }

  private onDeleteClick_() {
    // TODO(crbug.com/1350947): Show delete dialog if credential is present in
    // both stores.
    PasswordManagerImpl.getInstance().removeSavedPassword(
        this.password.id, this.password.storedIn);
    this.dispatchEvent(new CustomEvent('password-removed', {
      bubbles: true,
      composed: true,
      detail: {
        removedFromStores: this.password.storedIn,
      },
    }));
  }

  private showToast_(message: string) {
    this.toastMessage_ = message;
    this.$.toast.show();
  }

  private onEditClicked_() {
    this.showEditPasswordDialog_ = true;
    this.extendAuthValidity_();
  }

  private onEditPasswordDialogClosed_() {
    this.showEditPasswordDialog_ = false;
    this.extendAuthValidity_();
  }

  private getNoteValue_(): string {
    return !this.password.note ? this.i18n('emptyNote') : this.password.note!;
  }

  private isNoteFullyVisible_(): boolean {
    return this.showNoteFully_ ||
        this.$.noteValue.scrollHeight === this.$.noteValue.offsetHeight;
  }

  private onshowMoreClick_(e: Event) {
    e.preventDefault();
    this.showNoteFully_ = true;
    this.extendAuthValidity_();
  }

  private extendAuthValidity_() {
    PasswordManagerImpl.getInstance().extendAuthValidity();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'password-details-card': PasswordDetailsCardElement;
  }
}

customElements.define(
    PasswordDetailsCardElement.is, PasswordDetailsCardElement);
