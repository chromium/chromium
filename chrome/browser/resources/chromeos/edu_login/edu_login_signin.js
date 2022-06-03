// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './edu_login_css.js';
import './edu_login_template.js';
import './edu_login_button.js';
import './gaia_action_buttons.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AuthCompletedCredentials, Authenticator, AuthParams} from '../../gaia_auth_host/authenticator.m.js';
import {EduAccountLoginBrowserProxy, EduAccountLoginBrowserProxyImpl} from './browser_proxy.js';
import {EduLoginParams} from './edu_login_util.js';

Polymer({
  is: 'edu-login-signin',

  _template: html`{__html_template__}`,

  behaviors: [WebUIListenerBehavior],

  properties: {
    /**
     * Login params containing obfuscated Gaia id and Reauth Proof Token of the
     * parent who is approving EDU login flow.
     * @type {?EduLoginParams}
     */
    loginParams: Object,

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
    authExtHost_: Object,
  },

  /** @private {?EduAccountLoginBrowserProxy} */
  browserProxy_: null,

  /** @private {boolean} */
  enableGaiaActionButtons_: false,

  /** @override */
  created() {
    this.browserProxy_ = EduAccountLoginBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready() {
    this.authExtHost_ = new Authenticator(
      /** @type {!WebView} */(this.$.signinFrame));
    this.addAuthExtHostListeners_();
    this.browserProxy_.loginInitialize();
  },

  /** @override */
  attached() {
    this.addWebUIListener(
        'load-auth-extension', data => this.loadAuthExtension_(data));
    this.addWebUIListener('close-dialog', () => this.closeDialog_());
  },

  /** @private */
  addAuthExtHostListeners_() {
    this.authExtHost_.addEventListener('dropLink', e => this.onDropLink_(
        /** @type {!CustomEvent<string>} */(e)));
    this.authExtHost_.addEventListener(
        'newWindow', e => this.onNewWindow_(e));
    this.authExtHost_.addEventListener('ready', () => this.onAuthReady_());
    this.authExtHost_.addEventListener('resize', e => this.onResize_(
        /** @type {!CustomEvent<string>} */(e)));
    this.authExtHost_.addEventListener(
        'authCompleted', e => this.onAuthCompleted_(
            /** @type {!CustomEvent<!AuthCompletedCredentials>} */(e)));
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
   * @param {!Event} e
   * @private
   */
  onNewWindow_(e) {
    window.open(e.detail.targetUrl, '_blank');
    e.detail.window.discard();
  },

  /** @private */
  onAuthReady_() {
    this.loading_ = false;
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
    assert(this.loginParams);
    this.browserProxy_.completeLogin(e.detail, this.loginParams);
    this.loading_ = true;
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
    this.enableGaiaActionButtons_ = data.enableGaiaActionButtons;
  },

  /**
   * Closes the login dialog.
   * @private
   */
  closeDialog_() {
    this.browserProxy_.dialogClose();
  },

  /**
   * Navigate back in the web view if possible. Otherwise navigate to the
   * previous page.
   * @private
   */
  navigateBackInWebview_() {
    if (this.$.signinFrame.canGoBack()) {
      this.$.signinFrame.back();
      this.$.signinFrame.focus();
    } else {
      // Reload the webview. It allows users to go back and try to add another
      // account if something goes wrong in the webview (e.g. SAML server
      // doesn't respond, see crbug/1068783).
      this.reloadWebview_();
      this.fire('go-back');
    }
  },

  /**
   * Reloads the webview and shows 'loading' spinner.
   * @private
   */
  reloadWebview_() {
    this.loading_ = true;
    this.authExtHost_.resetStates();
    this.$.signinFrame.reload();
  },

  /**
   * @param {!Event} e
   * @private
   */
  handleGoBack_(e) {
    e.stopPropagation();
    this.navigateBackInWebview_();
  },

  /** @param {Authenticator} authExtHost */
  setAuthExtHostForTest(authExtHost) {
    this.authExtHost_ = authExtHost;
    this.addAuthExtHostListeners_();
  },
});
