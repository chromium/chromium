// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'confirm-password-change' is a dialog so that the user can
 * either confirm their old password, or confirm their new password (twice),
 * or both, as part of an in-session password change.
 * The dialog shows a spinner while it tries to change the password. This
 * spinner is also shown immediately in the case we are trying to change the
 * password using scraped data, and if this fails the spinner is hidden and
 * the main confirm dialog is shown.
 */

// TODO(crbug.com/40613129): Add logic to show only some of the passwords
// fields if some of the passwords were successfully scraped.

import 'chrome://confirm-password-change/strings.m.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';

import {sendWithPromise} from 'chrome://resources/ash/common/cr.m.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/ash/common/web_ui_listener_behavior.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './confirm_password_change.html.js';

/** @enum{number} */
const ValidationErrorType = {
  NO_ERROR: 0,
  MISSING_OLD_PASSWORD: 1,
  MISSING_NEW_PASSWORD: 2,
  MISSING_CONFIRM_NEW_PASSWORD: 3,
  PASSWORDS_DO_NOT_MATCH: 4,
  INCORRECT_OLD_PASSWORD: 5,
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const ConfirmPasswordChangeElementBase =
    mixinBehaviors([I18nBehavior, WebUIListenerBehavior], PolymerElement);

/**
 * @typedef {{
 *   dialog: CrDialogElement,
 * }}
 */
ConfirmPasswordChangeElementBase.$;

/** @polymer */
class ConfirmPasswordChangeElement extends ConfirmPasswordChangeElementBase {
  static get is() {
    return 'confirm-password-change';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** @private {boolean} */
      showSpinner_:
          {type: Boolean, value: true, observer: 'onShowSpinnerChanged_'},

      /** @private {boolean} */
      showOldPasswordPrompt_: {type: Boolean, value: true},

      /** @private {string} */
      oldPassword_: {type: String, value: ''},

      /** @private {boolean} */
      showNewPasswordPrompt_: {type: Boolean, value: true},

      /** @private {string} */
      newPassword_: {type: String, value: ''},

      /** @private {string} */
      confirmNewPassword_: {type: String, value: ''},

      /** @private {!ValidationErrorType} */
      currentValidationError_: {
        type: Number,
        value: ValidationErrorType.NO_ERROR,
        observer: 'onErrorChanged_',
      },

      /** @private {string} */
      promptString_: {
        type: String,
        computed:
            'getPromptString_(showOldPasswordPrompt_, showNewPasswordPrompt_)',
      },

      /** @private {string} */
      errorString_:
          {type: String, computed: 'getErrorString_(currentValidationError_)'},

    };
  }

  static get observers() {
    return [
      'onShowPromptChanged_(showOldPasswordPrompt_, showNewPasswordPrompt_)',

    ];
  }


  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.addWebUIListener('incorrect-old-password', () => {
      this.onIncorrectOldPassword_();
    });

    this.getInitialState_();
  }

  /** @private */
  getInitialState_() {
    sendWithPromise('getInitialState').then((result) => {
      this.showOldPasswordPrompt_ = result.showOldPasswordPrompt;
      this.showNewPasswordPrompt_ = result.showNewPasswordPrompt;
      this.showSpinner_ = result.showSpinner;
    });
  }


  /** @private */
  onShowSpinnerChanged_() {
    // Dialog is on top, spinner is underneath, so showing dialog hides spinner.
    if (this.showSpinner_) {
      this.$.dialog.close();
    } else {
      this.$.dialog.showModal();
    }
  }

  /** @private */
  onShowPromptChanged_() {
    const suffix = (this.showOldPasswordPrompt_ ? 'Old' : '') +
        (this.showNewPasswordPrompt_ ? 'New' : '');
    const width = loadTimeData.getInteger('width' + suffix);
    const height = loadTimeData.getInteger('height' + suffix);

    window.resizeTo(width, height);
  }

  /** @private */
  onErrorChanged_() {
    if (this.currentValidationError_ !== ValidationErrorType.NO_ERROR) {
      this.showSpinner_ = false;
    }
  }

  /** @private */
  onSaveTap_() {
    this.currentValidationError_ = this.findFirstError_();
    if (this.currentValidationError_ === ValidationErrorType.NO_ERROR) {
      chrome.send('changePassword', [this.oldPassword_, this.newPassword_]);
      this.showSpinner_ = true;
    }
  }

  /** @private */
  onIncorrectOldPassword_() {
    if (this.showOldPasswordPrompt_) {
      // User manually typed in the incorrect old password. Show the user an
      // incorrect password error and hide the spinner so they can try again.
      this.currentValidationError_ = ValidationErrorType.INCORRECT_OLD_PASSWORD;
    } else {
      // Until now we weren't showing the old password prompt, since we had
      // scraped the old password. But the password we scraped seems to be the
      // wrong one. So, start again, but this time ask for the old password too.
      this.showOldPasswordPrompt_ = true;
      this.currentValidationError_ = ValidationErrorType.MISSING_OLD_PASSWORD;
    }
  }

  /**
   * @return {!ValidationErrorType}
   * @private
   */
  findFirstError_() {
    if (this.showOldPasswordPrompt_) {
      if (!this.oldPassword_) {
        return ValidationErrorType.MISSING_OLD_PASSWORD;
      }
    }
    if (this.showNewPasswordPrompt_) {
      if (!this.newPassword_) {
        return ValidationErrorType.MISSING_NEW_PASSWORD;
      }
      if (!this.confirmNewPassword_) {
        return ValidationErrorType.MISSING_CONFIRM_NEW_PASSWORD;
      }
      if (this.newPassword_ !== this.confirmNewPassword_) {
        return ValidationErrorType.PASSWORDS_DO_NOT_MATCH;
      }
    }
    return ValidationErrorType.NO_ERROR;
  }

  /**
   * @return {boolean}
   * @private
   */
  invalidOldPassword_() {
    const err = this.currentValidationError_;
    return err === ValidationErrorType.MISSING_OLD_PASSWORD ||
        err === ValidationErrorType.INCORRECT_OLD_PASSWORD;
  }

  /**
   * @return {boolean}
   * @private
   */
  invalidNewPassword_() {
    return this.currentValidationError_ ===
        ValidationErrorType.MISSING_NEW_PASSWORD;
  }

  /**
   * @return {boolean}
   * @private
   */
  invalidConfirmNewPassword_() {
    const err = this.currentValidationError_;
    return err === ValidationErrorType.MISSING_CONFIRM_NEW_PASSWORD ||
        err === ValidationErrorType.PASSWORDS_DO_NOT_MATCH;
  }

  /**
   * @return {string}
   * @private
   */
  getPromptString_() {
    if (this.showOldPasswordPrompt_ && this.showNewPasswordPrompt_) {
      return this.i18n('bothPasswordsPrompt');
    }
    if (this.showOldPasswordPrompt_) {
      return this.i18n('oldPasswordPrompt');
    }
    if (this.showNewPasswordPrompt_) {
      return this.i18n('newPasswordPrompt');
    }
    return '';
  }

  /**
   * @return {string}
   * @private
   */
  getErrorString_() {
    switch (this.currentValidationError_) {
      case ValidationErrorType.INCORRECT_OLD_PASSWORD:
        return this.i18n('incorrectPassword');
      case ValidationErrorType.PASSWORDS_DO_NOT_MATCH:
        return this.i18n('matchError');
      default:
        return '';
    }
  }
}

customElements.define(
    ConfirmPasswordChangeElement.is, ConfirmPasswordChangeElement);
