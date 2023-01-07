// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'password-move-to-account-dialog' is the dialog that allows
 * moving a password stored on the user device to the account.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './avatar_icon.js';
import '../site_favicon.js';
import './passwords_shared.css.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl} from './password_manager_proxy.js';
import {getTemplate} from './password_move_to_account_dialog.html.js';

/**
 * This should be kept in sync with the enum in
 * components/password_manager/core/browser/password_manager_metrics_util.h.
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 */
export enum MoveToAccountStoreTrigger {
  SUCCESSFUL_LOGIN_WITH_PROFILE_STORE_PASSWORD = 0,
  EXPLICITLY_TRIGGERED_IN_SETTINGS = 1,
  EXPLICITLY_TRIGGERED_FOR_MULTIPLE_PASSWORDS_IN_SETTINGS = 2,
  USER_OPTED_IN_AFTER_SAVING_LOCALLY = 3,
  COUNT = 4,
}

export interface PasswordMoveToAccountDialogElement {
  $: {
    dialog: CrDialogElement,
    moveButton: HTMLElement,
  };
}

export class PasswordMoveToAccountDialogElement extends PolymerElement {
  static get is() {
    return 'password-move-to-account-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      passwordToMove: Object,
    };
  }

  passwordToMove: chrome.passwordsPrivate.PasswordUiEntry;

  override connectedCallback() {
    super.connectedCallback();

    chrome.send('metricsHandler:recordInHistogram', [
      'PasswordManager.AccountStorage.MoveToAccountStoreFlowOffered',
      MoveToAccountStoreTrigger.EXPLICITLY_TRIGGERED_IN_SETTINGS,
      MoveToAccountStoreTrigger.COUNT,
    ]);

    this.$.dialog.showModal();
  }

  private onMoveButtonClick_() {
    assert(
        this.passwordToMove.storedIn !==
        chrome.passwordsPrivate.PasswordStoreSet.ACCOUNT);
    PasswordManagerImpl.getInstance().movePasswordsToAccount(
        [this.passwordToMove.id]);
    this.$.dialog.close();
  }

  private onCancelButtonClick_() {
    this.$.dialog.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'password-move-to-account-dialog': PasswordMoveToAccountDialogElement;
  }
}

customElements.define(
    PasswordMoveToAccountDialogElement.is, PasswordMoveToAccountDialogElement);
