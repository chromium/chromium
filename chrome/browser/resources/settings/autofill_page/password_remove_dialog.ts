// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PasswordRemoveDialog is a dialog for choosing which copies of
 * a duplicated password to remove. A duplicated password is one that is stored
 * both on the device and in the account. If the user chooses to remove at least
 * one copy, a password-remove-dialog-passwords-removed event informs listeners
 * which copies were deleted.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import './avatar_icon.js';

import {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {I18nMixin} from 'chrome://resources/js/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SyncBrowserProxyImpl} from '../people_page/sync_browser_proxy.js';

import {MultiStorePasswordUiEntry} from './multi_store_password_ui_entry.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';
import {getTemplate} from './password_remove_dialog.html.js';

export type PasswordRemoveDialogPasswordsRemovedEvent =
    CustomEvent<{removedFromAccount: boolean, removedFromDevice: boolean}>;

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

  duplicatedPassword: MultiStorePasswordUiEntry;
  private removeFromAccountChecked_: boolean;
  private removeFromDeviceChecked_: boolean;
  private accountEmail_: string;

  override connectedCallback() {
    super.connectedCallback();

    // At creation time, the password should exist in both locations.
    assert(
        this.duplicatedPassword.isPresentInAccount() &&
        this.duplicatedPassword.isPresentOnDevice());
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
    const idsToRemove: Array<number> = [];
    if (this.removeFromAccountChecked_) {
      idsToRemove.push(this.duplicatedPassword.accountId!);
    }
    if (this.removeFromDeviceChecked_) {
      idsToRemove.push(this.duplicatedPassword.deviceId!);
    }
    PasswordManagerImpl.getInstance().removeSavedPasswords(idsToRemove);

    this.$.dialog.close();
    this.dispatchEvent(
        new CustomEvent('password-remove-dialog-passwords-removed', {
          bubbles: true,
          composed: true,
          detail: {
            removedFromAccount: this.removeFromAccountChecked_,
            removedFromDevice: this.removeFromDeviceChecked_,
          },
        }));
  }

  private onCancelButtonClick_() {
    this.$.dialog.close();
  }

  private shouldDisableRemoveButton_(): boolean {
    return !this.removeFromAccountChecked_ && !this.removeFromDeviceChecked_;
  }

  private getDialogBodyMessage_(): string {
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
