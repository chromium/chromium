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

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SyncBrowserProxyImpl} from '../people_page/sync_browser_proxy.js';

import {MultiStorePasswordUiEntry} from './multi_store_password_ui_entry.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';

/**
 * @typedef {!Event<!{removedFromAccount:boolean, removedFromDevice: boolean}>}
 */
export let PasswordRemoveDialogPasswordsRemovedEvent;

Polymer({
  is: 'password-remove-dialog',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /**
     * The password whose copies are to be removed.
     * @type {!MultiStorePasswordUiEntry}
     */
    duplicatedPassword: Object,

    /** @private */
    removeFromAccountChecked_: {
      type: Boolean,
      // Both checkboxes are selected by default (see |removeFromDeviceChecked_|
      // as well), since removing from both locations is the most common case.
      value: true,
    },

    /** @private */
    removeFromDeviceChecked_: {
      type: Boolean,
      value: true,
    },

    /** @private */
    accountEmail_: {
      type: String,
      value: '',
    },
  },

  /** @override */
  attached() {
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
  },

  /** @private */
  onRemoveButtonClick_() {
    /** @type{!Array<number>} */
    const idsToRemove = [];
    if (this.removeFromAccountChecked_) {
      idsToRemove.push(this.duplicatedPassword.accountId);
    }
    if (this.removeFromDeviceChecked_) {
      idsToRemove.push(this.duplicatedPassword.deviceId);
    }
    PasswordManagerImpl.getInstance().removeSavedPasswords(idsToRemove);

    this.$.dialog.close();
    this.fire('password-remove-dialog-passwords-removed', {
      removedFromAccount: this.removeFromAccountChecked_,
      removedFromDevice: this.removeFromDeviceChecked_
    });
  },

  /** @private */
  onCancelButtonClick_() {
    this.$.dialog.close();
  },

  /**
   * @private
   * @return {boolean}
   */
  shouldDisableRemoveButton_() {
    return !this.removeFromAccountChecked_ && !this.removeFromDeviceChecked_;
  },

  /**
   * @private
   * @return {string}
   */
  getDialogBodyMessage_() {
    return this.i18nAdvanced(
        'passwordRemoveDialogBody',
        {substitutions: [this.duplicatedPassword.urls.shown], tags: ['b']});
  },
});
