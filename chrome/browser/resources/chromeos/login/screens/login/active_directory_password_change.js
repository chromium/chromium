// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for Active Directory password change screen.
 */

/* #js_imports_placeholder */

(function() {

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

/**
 * UI mode for the dialog.
 * @enum {string}
 */
const UIState = {
  PASSWORD: 'password',
  PROGRESS: 'progress',
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 */
 const ActiveDirectoryPasswordChangeBase = Polymer.mixinBehaviors(
  [OobeI18nBehavior, OobeDialogHostBehavior,
   LoginScreenBehavior, MultiStepBehavior],
  Polymer.Element);

/**
 * @typedef {{
 *   errorDialog:  OobeModalDialogElement,
 *   oldPassword:  CrInputElement,
 *   newPassword:  CrInputElement,
 * }}
 */
ActiveDirectoryPasswordChangeBase.$;

class ActiveDirectoryPasswordChange extends ActiveDirectoryPasswordChangeBase {

  static get is() { return 'active-directory-password-change-element'; }

  /* #html_template_placeholder */

  static get properties() {
    return {
      /**
       * The user principal name.
       */
      username_: String,
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
       * The text content for error dialog.
       */
      errorDialogText_: String,
      /**
       * Indicates if old password is wrong.
       */
      oldPasswordWrong_: Boolean,
      /**
       * Indicates if new password is rejected.
       */
      newPasswordRejected_: Boolean,
      /**
       * Indicates if password is not repeated correctly.
       */
      passwordMismatch_: Boolean,
    };
  }

  constructor() {
    super();
    this.username_ = '';
    this.oldPassword_ = '';
    this.newPassword_ = '';
    this.newPasswordRepeat_ = '';
    this.errorDialogText_ = '';
    this.oldPasswordWrong_ = false;
    this.newPasswordRejected_ = false;
    this.passwordMismatch_ = false;
  }

  get EXTERNAL_API() {
    return ['showErrorDialog'];
  }

  defaultUIStep() {
    return UIState.PASSWORD;
  }

  static get UI_STEPS() {
    return UIState;
  }

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('ActiveDirectoryPasswordChangeScreen', {
      resetAllowed: false,
    });
  }

  /**
   * Event handler that is invoked just before the frame is shown.
   * @param {Object} data Screen init payload
   */
  onBeforeShow(data) {
    // Active Directory password change screen is similar to Active
    // Directory login screen. So we restore bottom bar controls.
    this.reset();
    if ('username' in data)
      this.username_ = data.username;
    if ('error' in data)
      this.setInvalid(data.error);
  }

  /**
   * Updates localized content of the screen that is not updated via
   * template.
   */
  updateLocalizedContent() {
    this.i18nUpdateLocale();
  }

  /**
   * @public
   * Shows sign-in error dialog.
   * @param {string} content Content to show in dialog.
   */
  showErrorDialog(content) {
    this.errorDialogText_ = content;
    this.$.errorDialog.showDialog();
  }

  /** @public */
  reset() {
    this.setUIStep(UIState.PASSWORD);
    this.resetInputFields_();
  }

  /** @private */
  resetInputFields_() {
    this.oldPassword_ = '';
    this.newPassword_ = '';
    this.newPasswordRepeat_ = '';
    this.errorDialogText_ = '';

    this.oldPasswordWrong_ = false;
    this.newPasswordRejected_ = false;
    this.passwordMismatch_ = false;
  }

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
  }

  /**
   *  @private
  */
  onSubmit_() {
    if (!this.$.oldPassword.validate() || !this.$.newPassword.validate()) {
      return;
    }
    if (this.newPassword_ != this.newPasswordRepeat_) {
      this.passwordMismatch_ = true;
      return;
    }
    this.setUIStep(UIState.PROGRESS);
    this.userActed(['changePassword', this.oldPassword_, this.newPassword_]);
    this.resetInputFields_();
  }

  /**
   * @private
   * Cancels password changing.
   */
  onClose_() {
    this.userActed('cancel');
  }
}

customElements.define(ActiveDirectoryPasswordChange.is, ActiveDirectoryPasswordChange);
})();

