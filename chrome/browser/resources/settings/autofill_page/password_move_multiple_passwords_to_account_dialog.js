// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'password-move-multiple-passwords-to-account-dialog' is the
 * dialog that allows moving multiple passwords stored on the user device to the
 * account.
 */

import './password_list_item.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MultiStorePasswordUiEntry} from './multi_store_password_ui_entry.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';
import {MoveToAccountStoreTrigger} from './password_move_to_account_dialog.js';

Polymer({
  is: 'password-move-multiple-passwords-to-account-dialog',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /** @type {!Array<!MultiStorePasswordUiEntry>} */
    passwordsToMove: {
      type: Array,
      value: () => [],
    },
    /** @private */
    movingPasswordsCountLabel_: {
      type: String,
      value: '',
    },
  },

  /** @override */
  attached() {
    chrome.metricsPrivate.recordEnumerationValue(
        'PasswordManager.AccountStorage.MoveToAccountStoreFlowOffered',
        MoveToAccountStoreTrigger
            .EXPLICITLY_TRIGGERED_FOR_MULTIPLE_PASSWORDS_IN_SETTINGS,
        MoveToAccountStoreTrigger.COUNT);
  },

  /** @private */
  onMoveButtonClick_() {
    const checkboxes = this.$.dialog.querySelectorAll('cr-checkbox');
    const selectedPasswords = [];
    checkboxes.forEach((checkbox) => {
      if (checkbox.checked) {
        selectedPasswords.push(Number(checkbox.dataset.id));
      }
    });
    PasswordManagerImpl.getInstance().movePasswordsToAccount(selectedPasswords);
    chrome.metricsPrivate.recordSmallCount(
        'PasswordManager.AccountStorage.MoveToAccountStorePasswordsCount',
        selectedPasswords.length);
    this.$.dialog.close();
  },

  /** @private */
  onCancelButtonClick_() {
    this.$.dialog.close();
  },

  updateMovingPasswordsCountLabel_() {
    const checkboxes = this.$.dialog.querySelectorAll('cr-checkbox');
    const selectedPasswordsCount =
        Array.from(checkboxes).filter(box => box.checked).length;

    PluralStringProxyImpl.getInstance()
        .getPluralString('movePasswordsToAccount', selectedPasswordsCount)
        .then(label => {
          this.movingPasswordsCountLabel_ = label;
        });
  },
});
