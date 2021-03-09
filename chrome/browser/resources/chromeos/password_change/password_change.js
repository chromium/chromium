// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An UI component to let user change their IdP password along
 * with cryptohome password.
 */

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import 'chrome://password-change/strings.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/icons.m.js';

Polymer({
  is: 'password-change',
  behaviors: [I18nBehavior],

  /**
   * The UI component that hosts IdP pages.
   * @type {!cr.samlPasswordChange.Authenticator|undefined}
   */
  authenticator_: undefined,

  /** @override */
  ready() {
    const signinFrame = this.getSigninFrame_();
    this.authenticator_ = new cr.samlPasswordChange.Authenticator(signinFrame);
    this.authenticator_.addEventListener(
        'authCompleted', this.onAuthCompleted_);
    chrome.send('initialize');
  },

  /**
   * Loads auth extension.
   * @param {Object} data Parameters for auth extension.
   */
  loadAuthExtension(data) {
    this.authenticator_.load(data);
  },

  /**
   * @return {!Element}
   * @private
   */
  getSigninFrame_() {
    // Note: Can't use |this.$|, since it returns cached references to elements
    // originally present in DOM, while the signin-frame is dynamically
    // recreated (see Authenticator.setWebviewPartition()).
    const signinFrame = this.shadowRoot.getElementById('signinFrame');
    assert(signinFrame);
    return signinFrame;
  },

  /** @private */
  onAuthCompleted_(e) {
    chrome.send(
        'changePassword', [e.detail.old_passwords, e.detail.new_passwords]);
  },

  /** @private */
  onCloseTap_() {
    chrome.send('dialogClose');
  },
});
