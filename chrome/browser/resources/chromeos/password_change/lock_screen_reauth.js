// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An UI component to let user init online re-auth flow on
 * the lock screen.
 */


import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';

const clearDataType = {
  appcache: true,
  cache: true,
  cookies: true,
};

Polymer({
  is: 'lock-reauth',
  behaviors: [I18nBehavior],

  properties: {
    // User non-canonicalized email for display
    email_: String,

    /**
     * Whether the ‘verify user’ screen is shown.
     */
    isVerifyUser_: {
      type: Boolean,
      value: true,
    },

    /**
     * Whether the ‘verify user again’ screen is shown.
     */
    isErrorDisplayed_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether user is authenticating on SAML page.
     */
    isSamlPage_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether there is a failure to scrape the user's password.
     */
    isConfirmPassword_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether no password is scraped or multiple passwords are scraped.
     */
    isManualInput_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether the user's password has changed.
     */
    isPasswordChanged_: {
      type: Boolean,
      value: false,
    },

    passwordConfirmAttempt_: {
      type: Number,
      value: 0,
    },

    passwordChangeAttempt_: {
      type: Number,
      value: 0,
    },
  },

  /**
   * The UI component that hosts IdP pages.
   * @type {!cr.login.Authenticator|undefined}
   */
  authenticator_: undefined,

  /**
   * Webview that view IdP page
   * @type {!webview|undefined}
   * @private
   */
  signinFrame_: undefined,

  /** @override */
  ready() {
    this.signinFrame_ = this.getSigninFrame_();
    this.authenticator_ = new cr.login.Authenticator(this.signinFrame_);
    this.authenticator_.addEventListener(
        'authDomainChange', this.onAuthDomainChange_.bind(this));
    this.authenticator_.addEventListener(
      'authCompleted', this.onAuthCompletedMessage_.bind(this));
    this.authenticator_.confirmPasswordCallback =
      this.onAuthConfirmPassword_.bind(this);
    this.authenticator_.noPasswordCallback = this.onAuthNoPassword_.bind(this);
    chrome.send('initialize');
  },

  /** @private */
  resetState_() {
    this.isVerifyUser_ = false;
    this.isErrorDisplayed_ = false;
    this.isSamlPage_ = false;
    this.isConfirmPassword_ = false;
    this.isManualInput_ = false;
    this.isPasswordChanged_ = false;
  },

  /**
   * Invoked when the authDomain property is changed on the authenticator.
   * @private
   */
  onAuthDomainChange_() {
    // <!--_html_template_start_-->
    this.$.samlNoticeMessage.textContent =
      loadTimeData.substituteString('$i18nPolymer{samlNotice}',
        this.authenticator_.authDomain);
    // <!--_html_template_end_-->
  },

  /**
   * Loads the authentication parameter into the iframe.
   * @param {!Object} data authenticator parameters bag.
   */
  loadAuthenticator(data) {
    this.authenticator_.setWebviewPartition(data.webviewPartitionName);
    let params = {};
    for (let i in cr.login.Authenticator.SUPPORTED_PARAMS) {
      const name = cr.login.Authenticator.SUPPORTED_PARAMS[i];
      if (data[name]) {
        params[name] = data[name];
      }
    }
    params.doSamlRedirect = true;
    this.authenticatorParams_ = params;
    this.email_ = data.email;
    chrome.send('authenticatorLoaded');
  },


  /**
   * This function is used when the wrong user is verified correctly
   * It reset authenticator state and display error message.
   */
  resetAuthenticator() {
    this.signinFrame_.clearData({since: 0}, clearDataType, () => {
      this.authenticator_.resetStates();
      this.isButtonsEnabled_ = true;
      this.isErrorDisplayed_ = true;
    });
  },

  /**
   * @return {!Element}
   * @private
   */
  getSigninFrame_() {
    // Note: Can't use |this.$|, since it returns cached references to elements
    // originally present in DOM, while the signin-frame is dynamically
    // recreated (see Authenticator.setWebviewPartition()).
    const signinFrame = this.shadowRoot.getElementById('signin-frame');
    assert(signinFrame);
    return signinFrame;
  },

  onAuthCompletedMessage_(e) {
    let credentials = e.detail;
    chrome.send('completeAuthentication', [
      credentials.gaiaId, credentials.email, credentials.password,
      credentials.usingSAML, credentials.services,
      credentials.passwordAttributes
    ]);
  },

  /**
   * Invoked when the user has successfully authenticated via SAML,
   * the Chrome Credentials Passing API was not used and the authenticator needs
   * the user to confirm the scraped password.
   * @param {string} email The authenticated user's e-mail.
   * @param {number} passwordCount The number of passwords that were scraped.
   * @private
   */
  onAuthConfirmPassword_(email, passwordCount) {
    this.resetState_();
    /** This statement override resetState_ calls.
     * Thus have to be AFTER resetState_. */
    this.isConfirmPassword_ = true;
    if (this.passwordConfirmAttempt_ > 0) {
      this.$.passwordInput.value = '';
      this.$.passwordInput.invalid = true;
    }
  },

  /**
   * Invoked when the user has successfully authenticated via SAML, the
   * Chrome Credentials Passing API was not used and no passwords
   * could be scraped.
   * The user will be asked to pick a manual password for the device.
   * @param {string} email The authenticated user's e-mail.
   * @private
   */
  onAuthNoPassword_(email) {
    this.resetState_();
    /** These two statement override resetState_ calls.
     * Thus have to be AFTER resetState_. */
    this.isConfirmPassword_ = true;
    this.isManualInput_ = true;
  },

  /**
   * Invoked when the dialog where the user enters a manual password for the
   * device, when password scraping fails.
   * @param {string} password The password the user entered. Not necessarily
   *     the same as their SAML password.
   * @private
   */
  onManualPasswordCollected(password) {
    this.authenticator_.completeAuthWithManualPassword(password);
  },

  /**
   * Invoked when the confirm password screen is dismissed.
   * @param {string} password The password entered at the confirm screen.
   * @private
   */
  onConfirmPasswordCollected(password) {
    this.passwordConfirmAttempt_++;
    this.authenticator_.verifyConfirmedPassword(password);
  },

  /**
   * Invoked when the user's password doesn't match his old password.
   * @private
   */
  passwordChanged() {
    this.resetState_();
    this.isPasswordChanged_ = true;
    this.passwordChangeAttempt_++;
    if (this.passwordChangeAttempt_ > 1) {
      this.$.oldPasswordInput.invalid = true;
    }
  },

  /** @private */
  onVerify_() {
    this.authenticator_.load(
      cr.login.Authenticator.AuthMode.DEFAULT, this.authenticatorParams_);
    this.resetState_();
    /** This statement override resetStates_ calls.
     * Thus have to be AFTER resetState_. */
    this.isSamlPage_ = true;
  },

  /** @private */
  onConfirm_() {
    if (!this.$.passwordInput.validate())
      return;
    if (this.isManualInput_) {
      // When using manual password entry, both passwords must match.
      let confirmPasswordInput = this.$$('#confirmPasswordInput');
      if (!confirmPasswordInput.validate())
        return;

      if (confirmPasswordInput.value != this.$.passwordInput.value) {
        this.$.passwordInput.invalid = true;
        confirmPasswordInput.invalid = true;
        return;
      }
    }

    if (this.isManualInput_) {
      this.onManualPasswordCollected(this.$.passwordInput.value);
    } else {
      this.onConfirmPasswordCollected(this.$.passwordInput.value);
    }
  },

  /** @private */
  onCloseTap_() {
    chrome.send('dialogClose');
  },

  onResetAndClose_() {
    this.signinFrame_.clearData({since: 0}, clearDataType, () => {
      onCloseTap_();
    });
  },

  /** @private */
  onNext_() {
    if (!this.$.oldPasswordInput.validate()) {
      this.$.oldPasswordInput.focusInput();
      return;
    }
    chrome.send('updateUserPassword', [this.$.oldPasswordInput.value]);
    this.$.oldPasswordInput.value = '';
  },

  /** @private */
  passwordPlaceholder_(locale, isManualInput_) {
    // <!--_html_template_start_-->
    return isManualInput_ ?
      '$i18n{manualPasswordInputLabel}' :
      '$i18n{confirmPasswordLabel}';
    // <!--_html_template_end_-->
  },

  /** @private */
  passwordErrorText_(locale, isManualInput_) {
    // <!--_html_template_start_-->
    return isManualInput_ ?
      '$i18n{manualPasswordMismatch}' :
      '$i18n{passwordChangedIncorrectOldPassword}';
    // <!--_html_template_end_-->
  },

});
