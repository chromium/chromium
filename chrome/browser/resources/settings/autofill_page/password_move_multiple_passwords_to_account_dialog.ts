// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'password-move-multiple-passwords-to-account-dialog' is the
 * dialog that allows moving multiple passwords stored on the user device to the
 * account.
 */

import './avatar_icon.js';
import './password_list_item.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import './passwords_shared.css.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl} from './password_manager_proxy.js';
import {getTemplate} from './password_move_multiple_passwords_to_account_dialog.html.js';

/**
 * This should be kept in sync with the enum in
 * components/password_manager/core/browser/password_manager_metrics_util.h.
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 */
enum MoveToAccountStoreTrigger {
  SUCCESSFUL_LOGIN_WITH_PROFILE_STORE_PASSWORD = 0,
  EXPLICITLY_TRIGGERED_IN_SETTINGS = 1,
  EXPLICITLY_TRIGGERED_FOR_MULTIPLE_PASSWORDS_IN_SETTINGS = 2,
  USER_OPTED_IN_AFTER_SAVING_LOCALLY = 3,
  COUNT = 4,
}

export interface PasswordMoveMultiplePasswordsToAccountDialogElement {
  $: {
    dialog: CrDialogElement,
    moveButton: HTMLElement,
    cancelButton: HTMLElement,
  };
}

export class PasswordMoveMultiplePasswordsToAccountDialogElement extends
    PolymerElement {
  static get is() {
    return 'password-move-multiple-passwords-to-account-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      passwordsToMove: {
        type: Array,
        value: () => [],
      },

      accountEmail: String,
    };
  }

  passwordsToMove: chrome.passwordsPrivate.PasswordUiEntry[];
  accountEmail: string;

  /** @return Whether the user confirmed the dialog. */
  wasConfirmed(): boolean {
    return this.$.dialog.getNative().returnValue === 'success';
  }

  override connectedCallback() {
    super.connectedCallback();

    chrome.metricsPrivate.recordEnumerationValue(
        'PasswordManager.AccountStorage.MoveToAccountStoreFlowOffered',
        MoveToAccountStoreTrigger
            .EXPLICITLY_TRIGGERED_FOR_MULTIPLE_PASSWORDS_IN_SETTINGS,
        MoveToAccountStoreTrigger.COUNT);
  }

  private onMoveButtonClick_() {
    const checkboxes = this.$.dialog.querySelectorAll('cr-checkbox');
    const selectedPasswords: number[] = [];
    checkboxes.forEach((checkbox) => {
      if (checkbox.checked) {
        selectedPasswords.push(Number(checkbox.dataset['id']));
      }
    });
    PasswordManagerImpl.getInstance().movePasswordsToAccount(selectedPasswords);
    this.$.dialog.close();
  }

  private onCancelButtonClick_() {
    this.$.dialog.cancel();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'password-move-multiple-passwords-to-account-dialog':
        PasswordMoveMultiplePasswordsToAccountDialogElement;
  }
}

customElements.define(
    PasswordMoveMultiplePasswordsToAccountDialogElement.is,
    PasswordMoveMultiplePasswordsToAccountDialogElement);
