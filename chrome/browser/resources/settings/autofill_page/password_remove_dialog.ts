// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PasswordRemoveDialog is a dialog for choosing which copies of
 * a duplicated password to remove. A duplicated password is one that is stored
 * both on the device and in the account. If the user chooses to remove at least
 * one copy, a password-remove-dialog-passwords-removed event informs listeners
 * which copies were deleted.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './avatar_icon.js';
import './passwords_shared.css.js';

import {SyncBrowserProxyImpl} from '/shared/settings/people_page/sync_browser_proxy.js';
import {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl} from './password_manager_proxy.js';
import {getTemplate} from './password_remove_dialog.html.js';

export type PasswordRemoveDialogPasswordsRemovedEvent =
    CustomEvent<{removedFromStores: chrome.passwordsPrivate.PasswordStoreSet}>;

declare global {
  interface HTMLElementEventMap {
    'password-remove-dialog-passwords-removed':
        PasswordRemoveDialogPasswordsRemovedEvent;
  }
}

export interface PasswordRemoveDialogElement {
  $: {
    dialog: CrDialogElement,
    removeButton: HTMLElement,
    removeFromAccountCheckbox: CrCheckboxElement,
    removeFromDeviceCheckbox: CrCheckboxElement,
  };
}

const PasswordRemoveDialogElementBase = I18nMixin(PolymerElement);

export class PasswordRemoveDialogElement extends
    PasswordRemoveDialogElementBase {
  static get is() {
    return 'password-remove-dialog';
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

      accountEmail_: {
        type: String,
        value: '',
      },
    };
  }

  duplicatedPassword: chrome.passwordsPrivate.PasswordUiEntry;
  private removeFromAccountChecked_: boolean;
  private removeFromDeviceChecked_: boolean;
  private accountEmail_: string;

  override connectedCallback() {
    super.connectedCallback();

    // At creation time, the password should exist in both locations.
    assert(
        this.duplicatedPassword.storedIn ===
        chrome.passwordsPrivate.PasswordStoreSet.DEVICE_AND_ACCOUNT);

    this.$.dialog.showModal();

    SyncBrowserProxyImpl.getInstance().getStoredAccounts().then(accounts => {
      // TODO(victorvianna): These checks just make testing easier because then
      // there's no need to wait for getStoredAccounts() to resolve. Remove them
      // and adapt the tests instead.
      if (!!accounts && accounts.length > 0) {
        this.accountEmail_ = accounts[0].email;
      }
    });
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

    this.$.dialog.close();
    this.dispatchEvent(
        new CustomEvent('password-remove-dialog-passwords-removed', {
          bubbles: true,
          composed: true,
          detail: {
            removedFromStores: fromStores,
          },
        }));
  }

  private onCancelButtonClick_() {
    this.$.dialog.close();
  }

  private shouldDisableRemoveButton_(): boolean {
    return !this.removeFromAccountChecked_ && !this.removeFromDeviceChecked_;
  }

  private getDialogBodyMessage_(): TrustedHTML {
    return this.i18nAdvanced(
        'passwordRemoveDialogBody',
        {substitutions: [this.duplicatedPassword.urls.shown], tags: ['b']});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'password-remove-dialog': PasswordRemoveDialogElement;
  }
}

customElements.define(
    PasswordRemoveDialogElement.is, PasswordRemoveDialogElement);
