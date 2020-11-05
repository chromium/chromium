// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for Active Directory password change screen.
 */

'use strict';

(function() {

/**
 * Horizontal padding for the error bubble.
 * @type {number}
 * @const
 */
const BUBBLE_HORIZONTAL_PADDING = 65;

/**
 * Vertical padding for the error bubble.
 * @type {number}
 * @const
 */
const BUBBLE_VERTICAL_PADDING = -144;

/**
 * Possible error states of the screen. Must be in the same order as
 * ActiveDirectoryPasswordChangeErrorState enum values.
 * @enum {number}
 */
const ACTIVE_DIRECTORY_PASSWORD_CHANGE_ERROR_STATE = {
  NO_ERROR: 0,
  WRONG_OLD_PASSWORD: 1,
  NEW_PASSWORD_REJECTED: 2,
};

Polymer({
  is: 'active-directory-password-change-element',

  behaviors: [OobeI18nBehavior, LoginScreenBehavior],

  EXTERNAL_API: [],

  properties: {
    /**
     * The user principal name.
     */
    username: String,

    /**
     * The old password.
     */
    oldPassword_: String,
    /**
     * The new password (first copy).
     */
    newPassword_: String,
    /**
     * The new password (second copy).
     */
    newPasswordRepeat_: String,

    /**
     * Indicates if old password is wrong.
     */
    oldPasswordWrong_: {
      type: Boolean,
      value: false,
    },
    /**
     * Indicates if new password is rejected.
     */
    newPasswordRejected_: {
      type: Boolean,
      value: false,
    },
    /**
     * Indicates if password is not repeated correctly.
     */
    passwordMismatch_: {
      type: Boolean,
      value: false,
    },
  },

  /** @override */
  ready() {
    this.initializeLoginScreen('ActiveDirectoryPasswordChangeScreen', {
      resetAllowed: false,
    });
  },

  /**
   * Event handler that is invoked just before the frame is shown.
   * @param {Object} data Screen init payload
   */
  onBeforeShow(data) {
    // Active Directory password change screen is similar to Active
    // Directory login screen. So we restore bottom bar controls.
    this.reset();
    if ('username' in data)
      this.username = data.username;
    if ('error' in data)
      this.setInvalid(data.error);
  },

  /**
   * Updates localized content of the screen that is not updated via
   * template.
   */
  updateLocalizedContent() {
    this.i18nUpdateLocale();
  },

  /**
   * Shows sign-in error bubble.
   * @param {number} loginAttempts Number of login attempts tried.
   * @param {HTMLElement} error Content to show in bubble.
   * @suppress {missingProperties}
   */
  showErrorBubble(loginAttempts, error) {
    $('bubble').showContentForElement(
        this, cr.ui.Bubble.Attachment.BOTTOM, error, BUBBLE_HORIZONTAL_PADDING,
        BUBBLE_VERTICAL_PADDING);
  },

  /** @public */
  reset() {
    this.$.animatedPages.selected = 0;
    this.resetInputFields_();
    this.updateNavigation_();
  },

  /** @private */
  resetInputFields_() {
    this.oldPassword = '';
    this.newPassword = '';
    this.newPasswordRepeat = '';

    this.oldPasswordWrong_ = false;
    this.newPasswordRejected_ = false;
    this.passwordMismatch_ = false;
  },

  /**
   * @public
   *  Invalidates a password input. Either the input for old password or for new
   *  password depending on passed error.
   * @param {ACTIVE_DIRECTORY_PASSWORD_CHANGE_ERROR_STATE} error
   */
  setInvalid(error) {
    switch (error) {
      case ACTIVE_DIRECTORY_PASSWORD_CHANGE_ERROR_STATE.NO_ERROR:
        break;
      case ACTIVE_DIRECTORY_PASSWORD_CHANGE_ERROR_STATE.WRONG_OLD_PASSWORD:
        this.oldPasswordWrong_ = true;
        break;
      case ACTIVE_DIRECTORY_PASSWORD_CHANGE_ERROR_STATE.NEW_PASSWORD_REJECTED:
        this.newPasswordRejected_ = true;
        break;
      default:
        console.error('Not handled error: ' + error);
    }
  },

  /** @private */
  onSubmit_() {
    if (!this.$.oldPassword.validate() || !this.$.newPassword.validate()) {
      return;
    }
    if (this.newPassword != this.newPasswordRepeat) {
      this.passwordMismatch_ = true;
      return;
    }
    this.$.animatedPages.selected++;
    this.updateNavigation_();
    this.resetInputFields_();
    chrome.send(
        'login.ActiveDirectoryPasswordChangeScreen.changePassword',
        [this.oldPassword, this.newPassword]);
  },

  /**
   * @private
   * Cancels password changing.
   */
  onClose_() {
    if (!this.$.navigation.closeVisible)
      return;
    this.userActed('cancel');
  },

  /** @private */
  updateNavigation_() {
    this.$.navigation.closeVisible = (this.$.animatedPages.selected == 0);
  },
});
})();
