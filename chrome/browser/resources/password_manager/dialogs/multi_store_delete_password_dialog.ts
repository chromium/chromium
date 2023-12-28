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
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../shared_style.css.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl} from '../password_manager_proxy.js';
import {UserUtilMixin} from '../user_utils_mixin.js';

import {getTemplate} from './multi_store_delete_password_dialog.html.js';

export interface MultiStoreDeletePasswordDialogElement {
  $: {
    dialog: CrDialogElement,
    removeButton: CrButtonElement,
    removeFromAccountCheckbox: CrCheckboxElement,
    removeFromDeviceCheckbox: CrCheckboxElement,
  };
}

const MultiStoreDeletePasswordDialogElementBase =
    UserUtilMixin(I18nMixin(PolymerElement));

export class MultiStoreDeletePasswordDialogElement extends
    MultiStoreDeletePasswordDialogElementBase {
  static get is() {
    return 'multi-store-delete-password-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The password whose copies are to be removed.
       */
      duplicatedPassword: Object,

      removeFromAccountChecked_: {
        type: Boolean,
        // Both checkboxes are selected by default (see
        // |removeFromDeviceChecked_| as well), since removing from both
        // locations is the most common case.
        value: true,
      },

      removeFromDeviceChecked_: {
        type: Boolean,
        value: true,
      },
    };
  }

  duplicatedPassword: chrome.passwordsPrivate.PasswordUiEntry;
  private removeFromAccountChecked_: boolean;
  private removeFromDeviceChecked_: boolean;

  override connectedCallback() {
    super.connectedCallback();

    // At creation time, the password should exist in both locations.
    assert(
        this.duplicatedPassword.storedIn ===
        chrome.passwordsPrivate.PasswordStoreSet.DEVICE_AND_ACCOUNT);

    this.$.dialog.showModal();
  }

  private onRemoveButtonClick_() {
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
    this.dispatchEvent(new CustomEvent('password-removed', {
      bubbles: true,
      composed: true,
      detail: {
        removedFromStores: fromStores,
      },
    }));
    this.$.dialog.close();
  }

  private onCancelButtonClick_() {
    this.$.dialog.close();
  }

  private shouldDisableRemoveButton_(): boolean {
    return !this.removeFromAccountChecked_ && !this.removeFromDeviceChecked_;
  }

  private getDialogBodyMessage_(): TrustedHTML {
    assert(this.duplicatedPassword.affiliatedDomains);

    return this.i18nAdvanced('deletePasswordDialogBody', {
      substitutions:
          [this.duplicatedPassword.affiliatedDomains.map(domain => domain.name)
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
