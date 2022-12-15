// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for Active Directory password change screen.
 */

import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.m.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/common_styles/oobe_common_styles.m.js';
import '../../components/common_styles/oobe_dialog_host_styles.m.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/dialogs/oobe_loading_dialog.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.m.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.m.js';
import {OobeDialogHostBehavior} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OobeModalDialog} from '../../components/dialogs/oobe_modal_dialog.js';


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
const ActiveDirectoryPasswordChangeBase = mixinBehaviors(
    [
      OobeI18nBehavior,
      OobeDialogHostBehavior,
      LoginScreenBehavior,
      MultiStepBehavior,
    ],
    PolymerElement);

/**
 * @typedef {{
 *   errorDialog:  OobeModalDialog,
 *   oldPassword:  CrInputElement,
 *   newPassword:  CrInputElement,
 * }}
 */
ActiveDirectoryPasswordChangeBase.$;

class ActiveDirectoryPasswordChange extends ActiveDirectoryPasswordChangeBase {
  static get is() {
    return 'active-directory-password-change-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

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
    this.initializeLoginScreen('ActiveDirectoryPasswordChangeScreen');
  }

  /**
   * Event handler that is invoked just before the frame is shown.
   * @param {Object} data Screen init payload
   */
  onBeforeShow(data) {
    // Active Directory password change screen is similar to Active
    // Directory login screen. So we restore bottom bar controls.
    this.reset();
    if ('username' in data) {
      this.username_ = data.username;
    }
    if ('error' in data) {
      this.setInvalid(data.error);
    }
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

customElements.define(
    ActiveDirectoryPasswordChange.is, ActiveDirectoryPasswordChange);
