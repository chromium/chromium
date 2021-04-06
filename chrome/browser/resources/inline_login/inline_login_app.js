// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';

import {isChromeOS} from '//resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {isRTL} from 'chrome://resources/js/util.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

// <if expr="chromeos">
import './gaia_action_buttons.js';
import './welcome_page_app.js';
import './strings.m.js';
// </if>

import {AuthCompletedCredentials, Authenticator, AuthParams} from '../gaia_auth_host/authenticator.m.js';
import {InlineLoginBrowserProxy, InlineLoginBrowserProxyImpl} from './inline_login_browser_proxy.js';

/**
 * @fileoverview Inline login WebUI in various signin flows for ChromeOS and
 * Chrome desktop (Windows only).
 */

/** @enum {string} */
const View = {
  addAccount: 'addAccount',
  welcome: 'welcome',
};

Polymer({
  is: 'inline-login-app',

  _template: html`{__html_template__}`,

  behaviors: [WebUIListenerBehavior],

  properties: {
    /** Mirroring the enum so that it can be used from HTML bindings. */
    View: {
      type: Object,
      value: View,
    },

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

    // <if expr="chromeos">
    /**
     * True if redesign of account management flows is enabled.
     * @private
     */
    isAccountManagementFlowsV2Enabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isAccountManagementFlowsV2Enabled');
      },
      readOnly: true,
    },

    /*
     * True if welcome page should not be shown.
     * @private
     */
    shouldSkipWelcomePage_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('shouldSkipWelcomePage');
      },
      readOnly: true,
    },

    /*
     * True if the dialog is open for reauthentication.
     * @private
     */
    isReauthentication_: {
      type: Boolean,
      value: false,
    },
    // </if>

    /**
     * Id of the screen that is currently displayed.
     * @private {View}
     */
    currentView_: {
      type: String,
      value: '',
    },
  },

  /** @private {?InlineLoginBrowserProxy} */
  browserProxy_: null,

  /**
   * Whether the login UI is loaded for signing in primary account.
   * @private {boolean}
   */
  isLoginPrimaryAccount_: false,

  /**
   * TODO(crbug.com/1164862): cleanup this flag, since it's enabled by default.
   * @private {boolean}
   */
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
    // <if expr="chromeos">
    if (this.isAccountManagementFlowsV2Enabled_) {
      // On Chrome OS this dialog is always-on-top, so we have to close it if
      // user opens a link in a new window.
      this.closeDialog_();
    }
    // </if>
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
    // Skip welcome page for reauthentication.
    if (data.email) {
      this.isReauthentication_ = true;
    }
    this.switchView_(this.getDefaultView_());
  },

  /**
   * Sends a message 'lstFetchResults'. This is a specific message sent when
   * the inline signin is loaded with reason kFetchLstOnly. Handlers of
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
    } else if (this.isWelcomePageEnabled_()) {
      // Allow user go back to the welcome page, if it's enabled.
      this.switchView_(View.welcome);
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

  /**
   * @return {boolean}
   * @private
   */
  shouldShowBackButton_() {
    return this.currentView_ === View.addAccount;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowOkButton_() {
    return this.currentView_ === View.welcome;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowGaiaButtons_() {
    return this.enableGaiaActionButtons_ &&
        this.currentView_ === View.addAccount;
  },

  /**
   * @return {View}
   * @private
   */
  getDefaultView_() {
    return this.isWelcomePageEnabled_() ? View.welcome : View.addAccount;
  },

  /**
   * @param {View} id identifier of the view that should be shown.
   * @private
   */
  switchView_(id) {
    this.currentView_ = id;
    /** @type {CrViewManagerElement} */ (this.$.viewManager).switchView(id);
  },

  /**
   * @return {boolean}
   * @private
   */
  isWelcomePageEnabled_() {
    if (!isChromeOS) {
      return false;
    }
    return this.isAccountManagementFlowsV2Enabled_ &&
        !this.shouldSkipWelcomePage_ && !this.isReauthentication_;
  },

  // <if expr="chromeos">
  /** @private */
  onOkButtonClick_() {
    this.switchView_(View.addAccount);
    const skipChecked =
        /** @type {WelcomePageAppElement} */ (this.$$('welcome-page-app'))
            .isSkipCheckboxChecked();
    this.browserProxy_.skipWelcomePage(skipChecked);
    this.setFocusToWebview_();
  },

  /** @private */
  setFocusToWebview_() {
    this.$.signinFrame.focus();
  },
  // </if>

  /** @param {Object} authExtHost */
  setAuthExtHostForTest(authExtHost) {
    this.authExtHost_ = /** @type {!Authenticator} */ (authExtHost);
    this.addAuthExtHostListeners_();
  },
});
