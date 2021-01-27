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
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';


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

    isErrorDisplayed_: {
      type: Boolean,
      value: false,
    },

    isButtonsEnabled_: {
      type: Boolean,
      value: true,
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
    chrome.send('initialize');
  },

  /**
   * Loads the authentication parameter into the iframe.
   * @param {!Object} data authenticator parameters bag.
   */
  LoadAuthenticatorParam(data) {
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

  /** @private */
  onVerify_() {
    this.authenticator_.load(
      cr.login.Authenticator.AuthMode.DEFAULT, this.authenticatorParams_);
    this.isButtonsEnabled_ = false;
    this.isVerifyUser_ = false;
    this.isErrorDisplayed_ = false;
  },

  /** @private */
  onCloseTap_() {
    chrome.send('dialogClose');
  },

});
