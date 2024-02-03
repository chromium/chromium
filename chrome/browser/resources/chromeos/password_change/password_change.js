// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An UI component to let user change their IdP password along
 * with cryptohome password.
 */

import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordChangeAuthenticator, PasswordChangeEventData} from '../../gaia_auth_host/password_change_authenticator.js';

import {getTemplate} from './password_change.html.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const PasswordChangeElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class PasswordChangeElement extends PasswordChangeElementBase {
  static get is() {
    return 'password-change';
  }

  static get template() {
    return getTemplate();
  }

  constructor() {
    super();

    /**
     * The UI component that hosts IdP pages.
     * @type {PasswordChangeAuthenticator|undefined}
     */
    this.authenticator_ = undefined;
  }

  /** @override */
  ready() {
    super.ready();
    const signinFrame = this.getSigninFrame_();
    this.authenticator_ = new PasswordChangeAuthenticator(signinFrame);
    this.authenticator_.addEventListener('authCompleted', (event) => {
      this.onAuthCompleted_(
          /**
           * @type {!CustomEvent<!PasswordChangeEventData>}
           */
          (event));
    });

    chrome.send('initialize');
  }

  /**
   * Loads auth extension.
   * @param {Object} data Parameters for auth extension.
   */
  loadAuthenticator(data) {
    this.authenticator_.load(data);
  }

  /**
   * @return {!WebView|string}
   * @private
   */
  getSigninFrame_() {
    // Note: Can't use |this.$|, since it returns cached references to elements
    // originally present in DOM, while the signin-frame is dynamically
    // recreated (see Authenticator.setWebviewPartition()).
    const signinFrame =
        /** @type {!WebView} */ (this.shadowRoot.getElementById('signinFrame'));
    assert(signinFrame);
    return signinFrame;
  }

  /**
   * @param {!CustomEvent<!PasswordChangeEventData>} e
   * @private
   * */
  onAuthCompleted_(e) {
    chrome.send(
        'changePassword', [e.detail.old_passwords, e.detail.new_passwords]);
  }

  /** @private */
  onCloseTap_() {
    chrome.send('dialogClose');
  }
}

customElements.define(PasswordChangeElement.is, PasswordChangeElement);
