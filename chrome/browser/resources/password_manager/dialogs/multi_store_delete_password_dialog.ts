// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview MultiStoreDeletePasswordDialog is a dialog for choosing which
 * copies of a duplicated password to remove. A duplicated password is one that
 * is stored both on the device and in the account.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {htmlEscape} from 'chrome://resources/js/util.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {PasswordManagerImpl} from '../password_manager_proxy.js';
import {UserUtilMixinLit} from '../user_utils_mixin_lit.js';

import {getCss} from './multi_store_delete_password_dialog.css.js';
import {getHtml} from './multi_store_delete_password_dialog.html.js';

export interface MultiStoreDeletePasswordDialogElement {
  $: {
    dialog: CrDialogElement,
    removeButton: CrButtonElement,
    removeFromAccountCheckbox: CrCheckboxElement,
    removeFromDeviceCheckbox: CrCheckboxElement,
  };
}

const MultiStoreDeletePasswordDialogElementBase =
    UserUtilMixinLit(I18nMixinLit(CrLitElement));

export class MultiStoreDeletePasswordDialogElement extends
    MultiStoreDeletePasswordDialogElementBase {
  static get is() {
    return 'multi-store-delete-password-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * The password whose copies are to be removed.
       */
      duplicatedPassword: {type: Object},

      // Both checkboxes are selected by default (see
      // |removeFromDeviceChecked_| as well), since removing from both
      // locations is the most common case.
      removeFromAccountChecked_: {type: Boolean},
      removeFromDeviceChecked_: {type: Boolean},
    };
  }

  accessor duplicatedPassword: chrome.passwordsPrivate.PasswordUiEntry;
  protected accessor removeFromAccountChecked_: boolean = true;
  protected accessor removeFromDeviceChecked_: boolean = true;

  override connectedCallback() {
    super.connectedCallback();

    // At creation time, the password should exist in both locations.
    assert(
        this.duplicatedPassword.storedIn ===
        chrome.passwordsPrivate.PasswordStoreSet.DEVICE_AND_ACCOUNT);

    this.$.dialog.showModal();
  }

  protected onRemoveFromAccountCheckedChanged_(
      e: CustomEvent<{value: boolean}>) {
    this.removeFromAccountChecked_ = e.detail.value;
  }

  protected onRemoveFromDeviceCheckedChanged_(
      e: CustomEvent<{value: boolean}>) {
    this.removeFromDeviceChecked_ = e.detail.value;
  }

  protected onRemoveButtonClick_() {
    let fromStores: chrome.passwordsPrivate.PasswordStoreSet =
        chrome.passwordsPrivate.PasswordStoreSet.DEVICE;
    if (this.removeFromAccountChecked_ && this.removeFromDeviceChecked_) {
      fromStores = chrome.passwordsPrivate.PasswordStoreSet.DEVICE_AND_ACCOUNT;
    } else if (this.removeFromAccountChecked_) {
      fromStores = chrome.passwordsPrivate.PasswordStoreSet.ACCOUNT;
    } else {
      assert(this.removeFromDeviceChecked_);
    }
    PasswordManagerImpl.getInstance().removeCredential(
        this.duplicatedPassword.id, fromStores);
    this.fire('password-removed', {
      removedFromStores: fromStores,
    });
    this.$.dialog.close();
  }

  protected onCancelButtonClick_() {
    this.$.dialog.close();
  }

  protected shouldDisableRemoveButton_(): boolean {
    return !this.removeFromAccountChecked_ && !this.removeFromDeviceChecked_;
  }

  protected getDialogBodyMessage_(): TrustedHTML {
    assert(this.duplicatedPassword.affiliatedDomains);

    return this.i18nAdvanced('deletePasswordDialogBody', {
      substitutions: [this.duplicatedPassword.affiliatedDomains
                          .map(domain => htmlEscape(domain.name))
                          .join(', ')],
      tags: ['b'],
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'multi-store-delete-password-dialog': MultiStoreDeletePasswordDialogElement;
  }
}

customElements.define(
    MultiStoreDeletePasswordDialogElement.is,
    MultiStoreDeletePasswordDialogElement);
