// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-users-add-user-dialog' is the dialog shown for adding new allowed
 * users to a ChromeOS device.
 */

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/ash/common/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './add_user_dialog.html.js';

/**
 * Regular expression for adding a user where the string provided is just
 * the part before the "@".
 * Email alias only, assuming it's a gmail address.
 *     e.g. 'john'
 */
const NAME_ONLY_REGEX =
    new RegExp('^\\s*([\\w\\.!#\\$%&\'\\*\\+-\\/=\\?\\^`\\{\\|\\}~]+)\\s*$');

/**
 * Regular expression for adding a user where the string provided is a full
 * email address.
 *     e.g. 'john@chromium.org'
 */
const EMAIL_REGEX = new RegExp(
    '^\\s*([\\w\\.!#\\$%&\'\\*\\+-\\/=\\?\\^`\\{\\|\\}~]+)@' +
    '([A-Za-z0-9\-]{2,63}\\..+)\\s*$');

enum UserAddError {
  NO_ERROR = 0,
  INVALID_EMAIL = 1,
  USER_EXISTS = 2,
}

const SettingsUsersAddUserDialogElementBase = I18nMixin(PolymerElement);

export interface SettingsUsersAddUserDialogElement {
  $: {
    dialog: CrDialogElement,
    addUserInput: CrInputElement,
  };
}

export class SettingsUsersAddUserDialogElement extends
    SettingsUsersAddUserDialogElementBase {
  static get is() {
    return 'settings-users-add-user-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      errorCode_: {
        type: Number,
        value: UserAddError.NO_ERROR,
      },

      isEmail_: {
        type: Boolean,
        value: false,
      },

      isEmpty_: {
        type: Boolean,
        value: true,
      },

    };
  }

  private errorCode_: UserAddError;
  private isEmail_: boolean;
  private isEmpty_: boolean;
  private usersPrivate_: typeof chrome.usersPrivate;

  constructor() {
    super();

    this.usersPrivate_ = chrome.usersPrivate;
  }

  open(): void {
    this.$.addUserInput.value = '';
    this.onInput_();
    this.$.dialog.showModal();
    // Set to valid initially since the user has not typed anything yet.
    this.$.addUserInput.invalid = false;
  }

  private async addUser_(): Promise<void> {
    // May be submitted by the Enter key even if the input value is invalid.
    if (this.$.addUserInput.disabled) {
      return;
    }

    const input: string = this.$.addUserInput.value;

    const nameOnlyMatches: RegExpExecArray|null = NAME_ONLY_REGEX.exec(input);
    let userEmail: string;
    if (nameOnlyMatches) {
      userEmail = nameOnlyMatches[1] + '@gmail.com';
    } else {
      const emailMatches = EMAIL_REGEX.exec(input);
      // Assuming the input validated, one of these two must match.
      assert(emailMatches);
      userEmail = emailMatches[1] + '@' + emailMatches[2];
    }
    const isUserInList: boolean =
        await this.usersPrivate_.isUserInList(userEmail);
    if (isUserInList) {
      // This user email had been saved previously
      this.errorCode_ = UserAddError.USER_EXISTS;
      return;
    }

    getAnnouncerInstance().announce(this.i18n('userAddedMessage', userEmail));

    this.$.dialog.close();
    this.usersPrivate_.addUser(userEmail);
    this.$.addUserInput.value = '';
  }

  private canAddUser_(): boolean {
    return this.isEmail_ && !this.isEmpty_;
  }

  private onCancelClick_(): void {
    this.$.dialog.cancel();
  }

  private onInput_(): void {
    const input = this.$.addUserInput.value;
    this.isEmail_ = NAME_ONLY_REGEX.test(input) || EMAIL_REGEX.test(input);
    this.isEmpty_ = input.length === 0;

    if (!this.isEmail_ && !this.isEmpty_) {
      this.errorCode_ = UserAddError.INVALID_EMAIL;
      return;
    }

    this.errorCode_ = UserAddError.NO_ERROR;
  }

  private shouldShowError_(): boolean {
    return this.errorCode_ !== UserAddError.NO_ERROR;
  }

  private getErrorString_(errorCode: UserAddError): string {
    if (errorCode === UserAddError.USER_EXISTS) {
      return this.i18n('userExistsError');
    }
    // TODO errorString for UserAddError.INVALID_EMAIL crbug/1007481

    return '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsUsersAddUserDialogElement.is]: SettingsUsersAddUserDialogElement;
  }
}

customElements.define(
    SettingsUsersAddUserDialogElement.is, SettingsUsersAddUserDialogElement);
