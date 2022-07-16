// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import './edu_login_css.js';
import './edu_login_template.js';
import './edu_login_button.js';
import './icons.js';

import {getImage} from 'chrome://resources/js/icon.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {EduAccountLoginBrowserProxyImpl} from './browser_proxy.js';
import {EduLoginErrorType, EduLoginParams, ParentAccount, ParentSigninFailureResult} from './edu_login_util.js';

Polymer({
  is: 'edu-login-parent-signin',

  _template: html`{__html_template__}`,

  properties: {
    /**
     * Parent who is going to sign-in.
     * @type {?ParentAccount}
     */
    parent: Object,

    /**
     * Login params containing obfuscated Gaia id and Reauth Proof Token of the
     * parent who is approving EDU login flow.
     * @type {?EduLoginParams}
     */
    loginParams: {
      type: Object,
      value: null,
      notify: true,
    },

    /**
     * Parent's password
     * @private {string}
     */
    password_: {
      type: String,
      value: '',
    },

    /** @private {boolean} */
    isPasswordVisible_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    isWrongPassword_: {
      type: Boolean,
      value: false,
    },
  },


  /**
   * Returns signin title with parent's name.
   * @return {string}
   * @private
   */
  getParentSigninTitle_() {
    return this.parent ?
        loadTimeData.getStringF('parentSigninTitle', this.parent.displayName) :
        '';
  },

  /**
   * Returns whether 'Next' button should be disabled. Returns true when
   * password is not empty.
   * @return {boolean}
   * @private
   */
  isNextDisabled_() {
    return !this.password_;
  },

  /**
   * Returns password field input type. Returns 'text' in case passVisible is
   * true, false otherwise.
   * @return {string}
   * @private
   */
  getInputType_() {
    return this.isPasswordVisible_ ? 'text' : 'password';
  },

  /**
   * Returns icon to hide/show the password.
   * @return {string}
   * @private
   */
  getPasswordIcon_() {
    return this.isPasswordVisible_ ? 'edu-login-icons:hide-password-icon' :
                                     'edu-login-icons:show-password-icon';
  },

  /**
   * Returns icon label to hide/show the password.
   * @return {string}
   * @private
   */
  getPasswordIconLabel_() {
    return loadTimeData.getString(
        this.isPasswordVisible_ ? 'parentSigninPasswordHide' :
                                  'parentSigninPasswordShow');
  },

  /**
   * Returns password field class. Returns 'error' in case wrongPassword is
   * true.
   * @return {string}
   * @private
   */
  getInputClass_() {
    return this.isWrongPassword_ ? 'error' : '';
  },

  /**
   * @param {string} iconUrl
   * @return {string} A CSS image-set for multiple scale factors.
   * @private
   */
  getIconImageSet_(iconUrl) {
    return iconUrl ? getImage(iconUrl) : '';
  },

  /**
   * @param {Event} e
   * @private
   */
  handleGoNext_(e) {
    e.stopPropagation();
    this.parentSignin_();
  },

  /**
   * Call 'parentSignin' with |password_| value.
   * @private
   */
  parentSignin_() {
    EduAccountLoginBrowserProxyImpl.getInstance()
        .parentSignin(this.parent, this.password_)
        .then(
            result => this.parentSigninSuccess_(result),
            result => this.parentSigninFailure_(result));
  },

  /**
   * Response to 'parentSignin' call. Called in case of successful fetch of
   * ReAuthProofToken.
   * @param {string} result The value of ReAuthProofToken.
   */
  parentSigninSuccess_(result) {
    this.loginParams = {
      reAuthProofToken: result,
      parentObfuscatedGaiaId: this.parent.obfuscatedGaiaId
    };
    this.fire('go-next');
  },

  /**
   * Response to 'parentSignin' call. Called in case of failed fetch of
   * ReAuthProofToken.
   * @param {!ParentSigninFailureResult} result has isWrongPassword property set
   *    to true in case when token fetching failed because of the wrong
   *    password.
   */
  parentSigninFailure_(result) {
    if (result && result.isWrongPassword) {
      this.isWrongPassword_ = true;
      this.$.passwordField.focus();
    } else {
      this.fire(
          'edu-login-error', {errorType: EduLoginErrorType.CANNOT_ADD_ACCOUNT});
    }
  },

  /** @private */
  onBackClicked_() {
    this.clearState_();
    this.fire('go-back');
  },

  /** @private */
  clearState_() {
    // Clear all values.
    this.password_ = '';
    this.isPasswordVisible_ = false;
    this.isWrongPassword_ = false;
  },

  /** @private */
  onTogglePasswordVisibilityClicked_() {
    this.isPasswordVisible_ = !this.isPasswordVisible_;
    this.$.passwordField.focus();
  },

  /**
   * Called on key down event on password field. Submits password on 'Enter'
   * key press.
   * @param {!Event} e
   * @private
   */
  onKeydown_(e) {
    if (e.key === 'Enter') {
      this.parentSignin_();
    }
  },

});
