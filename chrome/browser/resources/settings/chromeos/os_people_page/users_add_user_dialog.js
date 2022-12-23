// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-users-add-user-dialog' is the dialog shown for adding new allowed
 * users to a ChromeOS device.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import '../../settings_shared.css.js';
import '../../settings_vars.css.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './users_add_user_dialog.html.js';

/**
 * Regular expression for adding a user where the string provided is just
 * the part before the "@".
 * Email alias only, assuming it's a gmail address.
 *     e.g. 'john'
 * @type {!RegExp}
 */
const NAME_ONLY_REGEX =
    new RegExp('^\\s*([\\w\\.!#\\$%&\'\\*\\+-\\/=\\?\\^`\\{\\|\\}~]+)\\s*$');

/**
 * Regular expression for adding a user where the string provided is a full
 * email address.
 *     e.g. 'john@chromium.org'
 * @type {!RegExp}
 */
const EMAIL_REGEX = new RegExp(
    '^\\s*([\\w\\.!#\\$%&\'\\*\\+-\\/=\\?\\^`\\{\\|\\}~]+)@' +
    '([A-Za-z0-9\-]{2,63}\\..+)\\s*$');

/** @enum {number} */
const UserAddError = {
  NO_ERROR: 0,
  INVALID_EMAIL: 1,
  USER_EXISTS: 2,
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const SettingsUsersAddUserDialogElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class SettingsUsersAddUserDialogElement extends
    SettingsUsersAddUserDialogElementBase {
  static get is() {
    return 'settings-users-add-user-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** @private */
      errorCode_: {
        type: Number,
        value: UserAddError.NO_ERROR,
      },

      /** @private */
      isEmail_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      isEmpty_: {
        type: Boolean,
        value: true,
      },

    };
  }

  constructor() {
    super();

    /** @private */
    this.usersPrivate_ = chrome.usersPrivate;
  }

  open() {
    this.$.addUserInput.value = '';
    this.onInput_();
    this.$.dialog.showModal();
    // Set to valid initially since the user has not typed anything yet.
    this.$.addUserInput.invalid = false;
  }

  /** @private */
  addUser_() {
    // May be submitted by the Enter key even if the input value is invalid.
    if (this.$.addUserInput.disabled) {
      return;
    }

    const input = this.$.addUserInput.value;

    const nameOnlyMatches = NAME_ONLY_REGEX.exec(input);
    let userEmail;
    if (nameOnlyMatches) {
      userEmail = nameOnlyMatches[1] + '@gmail.com';
    } else {
      const emailMatches = EMAIL_REGEX.exec(input);
      // Assuming the input validated, one of these two must match.
      assert(emailMatches);
      userEmail = emailMatches[1] + '@' + emailMatches[2];
    }

    this.usersPrivate_.isUserInList(userEmail, isUserInList => {
      if (isUserInList) {
        // This user email had been saved previously
        this.errorCode_ = UserAddError.USER_EXISTS;
        return;
      }

      /** @type {!CrA11yAnnouncerElement} */ (getAnnouncerInstance())
          .announce(this.i18n('userAddedMessage', userEmail));

      this.$.dialog.close();
      this.usersPrivate_.addUser(
          userEmail,
          /* callback */ function(success) {});

      this.$.addUserInput.value = '';
    });
  }

  /**
   * @return {boolean}
   * @private
   */
  canAddUser_() {
    return this.isEmail_ && !this.isEmpty_;
  }

  /** @private */
  onCancelTap_() {
    this.$.dialog.cancel();
  }

  /** @private */
  onInput_() {
    const input = this.$.addUserInput.value;
    this.isEmail_ = NAME_ONLY_REGEX.test(input) || EMAIL_REGEX.test(input);
    this.isEmpty_ = input.length === 0;

    if (!this.isEmail_ && !this.isEmpty_) {
      this.errorCode_ = UserAddError.INVALID_EMAIL;
      return;
    }

    this.errorCode_ = UserAddError.NO_ERROR;
  }

  /**
   * @private
   * @return {boolean}
   */
  shouldShowError_() {
    return this.errorCode_ !== UserAddError.NO_ERROR;
  }

  /**
   * @private
   * @return {string}
   */
  getErrorString_(errorCode_) {
    if (errorCode_ === UserAddError.USER_EXISTS) {
      return this.i18n('userExistsError');
    }
    // TODO errorString for UserAddError.INVALID_EMAIL crbug/1007481

    return '';
  }
}

customElements.define(
    SettingsUsersAddUserDialogElement.is, SettingsUsersAddUserDialogElement);
