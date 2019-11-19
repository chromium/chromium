// Copyright 2018 The Chromium Authors. All rights reserved.
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

// TODO(https://crbug.com/930109): Add logic to show only some of the passwords
// fields if some of the passwords were successfully scraped.

/** @enum{number} */
const ValidationErrorType = {
  NO_ERROR: 0,
  MISSING_OLD_PASSWORD: 1,
  MISSING_NEW_PASSWORD: 2,
  MISSING_CONFIRM_NEW_PASSWORD: 3,
  PASSWORDS_DO_NOT_MATCH: 4,
  INCORRECT_OLD_PASSWORD: 5,
};

Polymer({
  is: 'confirm-password-change',

  behaviors: [I18nBehavior, WebUIListenerBehavior],

  properties: {
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
  },

  observers: [
    'onShowPromptChanged_(showOldPasswordPrompt_, showNewPasswordPrompt_)',
  ],

  /** @override */
  attached: function() {
    this.addWebUIListener('incorrect-old-password', () => {
      this.onIncorrectOldPassword_();
    });

    this.getInitialState_();
  },

  /** @private */
  getInitialState_() {
    cr.sendWithPromise('getInitialState').then((result) => {
      this.showOldPasswordPrompt_ = result.showOldPasswordPrompt;
      this.showNewPasswordPrompt_ = result.showNewPasswordPrompt;
      this.showSpinner_ = result.showSpinner;
    });
  },


  /** @private */
  onShowSpinnerChanged_: function() {
    // Dialog is on top, spinner is underneath, so showing dialog hides spinner.
    if (this.showSpinner_)
      this.$.dialog.close();
    else
      this.$.dialog.showModal();
  },

  /** @private */
  onShowPromptChanged_: function() {
    const suffix = (this.showOldPasswordPrompt_ ? 'Old' : '') +
        (this.showNewPasswordPrompt_ ? 'New' : '');
    const width = loadTimeData.getInteger('width' + suffix);
    const height = loadTimeData.getInteger('height' + suffix);

    window.resizeTo(width, height);
  },

  /** @private */
  onErrorChanged_: function() {
    if (this.currentValidationError_ != ValidationErrorType.NO_ERROR) {
      this.showSpinner_ = false;
    }
  },

  /** @private */
  onSaveTap_: function() {
    this.currentValidationError_ = this.findFirstError_();
    if (this.currentValidationError_ == ValidationErrorType.NO_ERROR) {
      chrome.send('changePassword', [this.oldPassword_, this.newPassword_]);
      this.showSpinner_ = true;
    }
  },

  /** @private */
  onIncorrectOldPassword_: function() {
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
  },

  /**
   * @return {!ValidationErrorType}
   * @private
   */
  findFirstError_: function() {
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
      if (this.newPassword_ != this.confirmNewPassword_) {
        return ValidationErrorType.PASSWORDS_DO_NOT_MATCH;
      }
    }
    return ValidationErrorType.NO_ERROR;
  },

  /**
   * @return {boolean}
   * @private
   */
  invalidOldPassword_: function() {
    const err = this.currentValidationError_;
    return err == ValidationErrorType.MISSING_OLD_PASSWORD ||
        err == ValidationErrorType.INCORRECT_OLD_PASSWORD;
  },

  /**
   * @return {boolean}
   * @private
   */
  invalidNewPassword_: function() {
    return this.currentValidationError_ ==
        ValidationErrorType.MISSING_NEW_PASSWORD;
  },

  /**
   * @return {boolean}
   * @private
   */
  invalidConfirmNewPassword_: function() {
    const err = this.currentValidationError_;
    return err == ValidationErrorType.MISSING_CONFIRM_NEW_PASSWORD ||
        err == ValidationErrorType.PASSWORDS_DO_NOT_MATCH;
  },

  /**
   * @return {string}
   * @private
   */
  getPromptString_: function() {
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
  },

  /**
   * @return {string}
   * @private
   */
  getErrorString_: function() {
    switch (this.currentValidationError_) {
      case ValidationErrorType.INCORRECT_OLD_PASSWORD:
        return this.i18n('incorrectPassword');
      case ValidationErrorType.PASSWORDS_DO_NOT_MATCH:
        return this.i18n('matchError');
      default:
        return '';
    }
  },
});
