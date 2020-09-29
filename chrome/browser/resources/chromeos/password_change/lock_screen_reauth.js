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


Polymer({
  is: 'lock-reauth',
  behaviors: [I18nBehavior],

  properties: {
    isErrorDisplayed_: {
      type: Boolean,
      value: false,
    },

    isButtonsEnabled_: {
      type: Boolean,
      value: true,
    },

    isVerifyButtonEnabled_: {
      type: Boolean,
      computed:
          'computeVerifyButtonEnabled_(isErrorDisplayed_,isButtonsEnabled_)',
    },

    isVerifyAgainButtonEnabled_: {
      type: Boolean,
      computed:
          'computeVerifyAgainButtonEnabled_(isErrorDisplayed_,isButtonsEnabled_)',
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
  attached() {
    this.$.dialog.showModal();
  },

  /** @override */
  ready() {
    this.signinFrame_ = this.getSigninFrame_();
    this.authenticator_ = new cr.login.Authenticator(this.signinFrame_);
    chrome.send('initialize');
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
  computeVerifyButtonEnabled_(isErrorDisplayed, isButtonsEnabled) {
    return !isErrorDisplayed && isButtonsEnabled;
  },

  /** @private */
  computeVerifyAgainButtonEnabled_(isErrorDisplayed, isButtonsEnabled) {
    return isErrorDisplayed && isButtonsEnabled;
  },

  /** @private */
  onNext_() {
    this.isButtonsEnabled_ = false;
  },

  /** @private */
  onCloseTap_() {
    chrome.send('dialogClose');
  },

});
