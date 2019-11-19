// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for Active Directory password change screen.
 */

/**
 * Possible error states of the screen. Must be in the same order as
 * ActiveDirectoryPasswordChangeErrorState enum values.
 * @enum {number}
 */
var ACTIVE_DIRECTORY_PASSWORD_CHANGE_ERROR_STATE = {
  WRONG_OLD_PASSWORD: 0,
  NEW_PASSWORD_REJECTED: 1,
};

Polymer({
  is: 'active-directory-password-change',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * The user principal name.
     */
    username: String,
  },

  /** @public */
  reset: function() {
    this.$.animatedPages.selected = 0;
    this.$.inputForm.reset();
    this.updateNavigation_();
  },

  /**
   * @public
   *  Invalidates a password input. Either the input for old password or for new
   *  password depending on passed error.
   * @param {ACTIVE_DIRECTORY_PASSWORD_CHANGE_ERROR_STATE} error
   */
  setInvalid: function(error) {
    switch (error) {
      case ACTIVE_DIRECTORY_PASSWORD_CHANGE_ERROR_STATE.WRONG_OLD_PASSWORD:
        this.$.oldPassword.isInvalid = true;
        break;
      case ACTIVE_DIRECTORY_PASSWORD_CHANGE_ERROR_STATE.NEW_PASSWORD_REJECTED:
        this.$.newPassword1.isInvalid = true;
        break;
      default:
        console.error('Not handled error: ' + error);
    }
  },

  /** @private */
  onSubmit_: function() {
    if (!this.$.oldPassword.checkValidity() ||
        !this.$.newPassword1.checkValidity()) {
      return;
    }
    if (this.$.newPassword1.value != this.$.newPassword2.value) {
      this.$.newPassword2.isInvalid = true;
      return;
    }
    this.$.animatedPages.selected++;
    this.updateNavigation_();
    var msg = {
      'username': this.username,
      'oldPassword': this.$.oldPassword.value,
      'newPassword': this.$.newPassword1.value,
    };
    this.$.oldPassword.value = '';
    this.$.newPassword1.value = '';
    this.$.newPassword2.value = '';
    this.fire('authCompleted', msg);
  },

  /** @private */
  onClose_: function() {
    if (!this.$.navigation.closeVisible)
      return;
    this.fire('cancel');
  },

  /** @private */
  updateNavigation_: function() {
    this.$.navigation.closeVisible = (this.$.animatedPages.selected == 0);
  },
});
