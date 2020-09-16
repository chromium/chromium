// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

// <if expr="chromeos">
import './gaia_action_buttons.js';
// </if>

import {isRTL} from 'chrome://resources/js/util.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AuthCompletedCredentials, Authenticator, AuthParams} from '../gaia_auth_host/authenticator.m.js';
import {InlineLoginBrowserProxy, InlineLoginBrowserProxyImpl} from './inline_login_browser_proxy.js';

/**
 * @fileoverview Inline login WebUI in various signin flows for ChromeOS and
 * Chrome desktop (Windows only).
 */

Polymer({
  is: 'inline-login-app',

  _template: html`{__html_template__}`,

  behaviors: [WebUIListenerBehavior],

  properties: {
    /**
     * Indicates whether the page is loading.
     * @private {boolean}
     */
    loading_: {
      type: Boolean,
      value: true,
    },

    /**
     * The auth extension host instance.
     * @private {?Authenticator}
     */
    authExtHost_: {
      type: Object,
      value: null,
    },
  },

  /** @private {?InlineLoginBrowserProxy} */
  browserProxy_: null,

  /**
   * Whether the login UI is loaded for signing in primary account.
   * @private {boolean}
   */
  isLoginPrimaryAccount_: false,

  /** @private {boolean} */
  enableGaiaActionButtons_: false,

  /** @override */
  created() {
    this.browserProxy_ = InlineLoginBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready() {
    this.authExtHost_ = new Authenticator(
        /** @type {!WebView} */ (this.$.signinFrame));
    this.addAuthExtHostListeners_();
    this.browserProxy_.initialize();
  },

  /** @override */
  attached() {
    this.addWebUIListener(
        'load-auth-extension', data => this.loadAuthExtension_(data));
    this.addWebUIListener(
        'send-lst-fetch-results', arg => this.sendLSTFetchResults_(arg));
    this.addWebUIListener('close-dialog', () => this.closeDialog_());
  },

  /** @private */
  addAuthExtHostListeners_() {
    this.authExtHost_.addEventListener(
        'dropLink',
        e => this.onDropLink_(
            /** @type {!CustomEvent<string>} */ (e)));
    this.authExtHost_.addEventListener(
        'newWindow',
        e => this.onNewWindow_(
            /** @type {!CustomEvent<NewWindowProperties>} */ (e)));
    this.authExtHost_.addEventListener('ready', () => this.onAuthReady_());
    this.authExtHost_.addEventListener(
        'resize',
        e => this.onResize_(
            /** @type {!CustomEvent<string>} */ (e)));
    this.authExtHost_.addEventListener(
        'authCompleted',
        e => this.onAuthCompleted_(
            /** @type {!CustomEvent<!AuthCompletedCredentials>} */ (e)));
    this.authExtHost_.addEventListener(
        'showIncognito', () => this.onShowIncognito_());
    this.authExtHost_.addEventListener(
        'getAccounts', () => this.onGetAccounts_());
  },

  /**
   * @param {!CustomEvent<string>} e
   * @private
   */
  onDropLink_(e) {
    // Navigate to the dropped link.
    window.location.href = e.detail;
  },

  /**
   * @param {!CustomEvent<NewWindowProperties>} e
   * @private
   */
  onNewWindow_(e) {
    window.open(e.detail.targetUrl, '_blank');
    e.detail.window.discard();
  },

  /** @private */
  onAuthReady_() {
    this.loading_ = false;
    if (this.isLoginPrimaryAccount_) {
      this.browserProxy_.recordAction('Signin_SigninPage_Shown');
    }
    this.browserProxy_.authExtensionReady();
  },

  /**
   * @param {!CustomEvent<string>} e
   * @private
   */
  onResize_(e) {
    this.browserProxy_.switchToFullTab(e.detail);
  },

  /**
   * @param {!CustomEvent<!AuthCompletedCredentials>} e
   * @private
   */
  onAuthCompleted_(e) {
    this.loading_ = true;
    this.browserProxy_.completeLogin(e.detail);
  },

  /** @private */
  onShowIncognito_() {
    this.browserProxy_.showIncognito();
  },

  /** @private */
  onGetAccounts_() {
    this.browserProxy_.getAccounts().then(result => {
      this.authExtHost_.getAccountsResponse(result);
    });
  },

  /**
   * Loads auth extension.
   * @param {!AuthParams} data Parameters for auth extension.
   * @private
   */
  loadAuthExtension_(data) {
    this.authExtHost_.load(data.authMode, data);
    this.loading_ = true;
    this.isLoginPrimaryAccount_ = data.isLoginPrimaryAccount;
    this.enableGaiaActionButtons_ = data.enableGaiaActionButtons;
  },

  /**
   * Sends a message 'lstFetchResults'. This is a specific message sent when
   * the inline signin is loaded with reason REASON_FETCH_LST_ONLY. Handlers of
   * this message would expect a single argument a base::Dictionary value that
   * contains the values fetched from the gaia sign in endpoint.
   * @param {string} arg The string representation of the json data returned by
   *    the sign in dialog after it has finished the sign in process.
   * @private
   */
  sendLSTFetchResults_(arg) {
    this.browserProxy_.lstFetchResults(arg);
  },

  /**
   * Closes the login dialog.
   * @private
   */
  closeDialog_() {
    this.browserProxy_.dialogClose();
  },

  /**
   * Navigates back in the web view if possible. Otherwise closes the dialog.
   * @private
   */
  handleGoBack_() {
    if (this.$.signinFrame.canGoBack()) {
      this.$.signinFrame.back();
      this.$.signinFrame.focus();
    } else {
      this.closeDialog_();
    }
  },

  /**
   * @return {string}
   * @private
   */
  getBackButtonIcon_() {
    return isRTL() ? 'cr:chevron-right' : 'cr:chevron-left';
  },

  /** @param {Object} authExtHost */
  setAuthExtHostForTest(authExtHost) {
    this.authExtHost_ = /** @type {!Authenticator} */ (authExtHost);
    this.addAuthExtHostListeners_();
  },
});
