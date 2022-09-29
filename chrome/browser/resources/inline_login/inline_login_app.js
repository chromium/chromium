// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/cr_elements/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {isRTL} from 'chrome://resources/js/util.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/cr_elements/web_ui_listener_behavior.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

// <if expr="chromeos_ash">
import './arc_account_picker/arc_account_picker_app.js';
import './gaia_action_buttons/gaia_action_buttons.js';
import './signin_blocked_by_policy_page.js';
import './signin_error_page.js';
import './welcome_page_app.js';
import './strings.m.js';
import {getAccountAdditionOptionsFromJSON} from './arc_account_picker/arc_util.js';
import {WelcomePageAppElement} from './welcome_page_app.js';
// </if>

import {AuthCompletedCredentials, Authenticator, AuthParams} from './gaia_auth_host/authenticator.js';
import {getTemplate} from './inline_login_app.html.js';
import {InlineLoginBrowserProxy, InlineLoginBrowserProxyImpl} from './inline_login_browser_proxy.js';

/**
 * @fileoverview Inline login WebUI in various signin flows for ChromeOS and
 * Chrome desktop (Windows only).
 */

/** @enum {string} */
export const View = {
  ADD_ACCOUNT: 'addAccount',
  SIGNIN_BLOCKED_BY_POLICY: 'signinBlockedByPolicy',
  SIGNIN_ERROR: 'signinError',
  WELCOME: 'welcome',
  ARC_ACCOUNT_PICKER: 'arcAccountPicker',
};


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {WebUIListenerBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 */
const InlineLoginAppElementBase =
    mixinBehaviors([WebUIListenerBehavior, I18nBehavior], PolymerElement);

/** @polymer */
export class InlineLoginAppElement extends InlineLoginAppElementBase {
  static get is() {
    return 'inline-login-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** Mirroring the enum so that it can be used from HTML bindings. */
      viewEnum_: {
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
       * Indicates whether the account is being verified.
       * @private {boolean}
       */
      verifyingAccount_: {
        type: Boolean,
        value: false,
      },

      /**
       * The auth extension host instance.
       * @private {?Authenticator}
       */
      authExtHost_: {
        type: Object,
        value: null,
      },

      // <if expr="chromeos_ash">
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
       * True if `kArcAccountRestrictions` feature is enabled.
       * @private
       */
      isArcAccountRestrictionsEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isArcAccountRestrictionsEnabled');
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

      /*
       * True if the account should be available in ARC++ after addition.
       * @private
       */
      isAvailableInArc_: {
        type: Boolean,
        value: false,
      },

      /**
       * User's email used in the sign-in flow.
       * @private {string}
       */
      email_: {type: String, value: ''},

      /**
       * Hosted domain of the user's email used in the sign-in flow.
       * @private {string}
       */
      hostedDomain_: {type: String, value: ''},

      /**
       * @return {boolean} True if secondary account sign-ins are allowed, false
       *    otherwise.
       * @private
       */
      isSecondaryGoogleAccountSigninAllowed_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('secondaryGoogleAccountSigninAllowed');
        },
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
    };
  }

  /** @override */
  constructor() {
    super();

    /**
     * Whether the login UI is loaded for signing in primary account.
     * @private {boolean}
     */
    this.isLoginPrimaryAccount_ = false;


    /** @private {InlineLoginBrowserProxy} */
    this.browserProxy_ = InlineLoginBrowserProxyImpl.getInstance();
  }

  /** @override */
  ready() {
    super.ready();

    // <if expr="chromeos_ash">
    if (!this.isSecondaryGoogleAccountSigninAllowed_) {
      // This can happen only if the user opened chrome://chrome-signin manually
      // in the browser. Normally (in the account addition dialog) this will be
      // handled earlier and a special error screen will be shown.
      console.error(
          'SecondaryGoogleAccountSigninAllowed is set to false - aborting.');
      return;
    }
    // </if>

    this.authExtHost_ = new Authenticator(
        /** @type {!WebView} */ (this.$.signinFrame));
    this.addAuthExtHostListeners_();
    this.browserProxy_.initialize();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.addWebUIListener(
        'load-auth-extension', data => this.loadAuthExtension_(data));
    this.addWebUIListener(
        'send-lst-fetch-results', arg => this.sendLSTFetchResults_(arg));
    this.addWebUIListener('close-dialog', () => this.closeDialog_());
    // <if expr="chromeos_ash">
    this.addWebUIListener(
        'show-signin-error-page', data => this.signinErrorShowView_(data));
    // </if>
  }

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
  }

  /**
   * @param {!CustomEvent<string>} e
   * @private
   */
  onDropLink_(e) {
    // Navigate to the dropped link.
    window.location.href = e.detail;
  }

  /**
   * @param {!CustomEvent<NewWindowProperties>} e
   * @private
   */
  onNewWindow_(e) {
    window.open(e.detail.targetUrl, '_blank');
    e.detail.window.discard();
    // <if expr="chromeos_ash">
    // On Chrome OS this dialog is always-on-top, so we have to close it if
    // user opens a link in a new window.
    this.closeDialog_();
    // </if>
  }

  /** @private */
  onAuthReady_() {
    this.loading_ = false;
    if (this.isLoginPrimaryAccount_) {
      this.browserProxy_.recordAction('Signin_SigninPage_Shown');
    }
    this.browserProxy_.authExtensionReady();
  }

  /**
   * @param {!CustomEvent<string>} e
   * @private
   */
  onResize_(e) {
    this.browserProxy_.switchToFullTab(e.detail);
  }

  /**
   * @param {!CustomEvent<!AuthCompletedCredentials>} e
   * @private
   */
  onAuthCompleted_(e) {
    this.verifyingAccount_ = true;
    /** @type {!AuthCompletedCredentials} */
    const credentials = e.detail;

    // <if expr="chromeos_ash">
    if (this.isArcAccountRestrictionsEnabled_ && !this.isReauthentication_) {
      credentials.isAvailableInArc = this.isAvailableInArc_;
    }
    // </if>

    this.browserProxy_.completeLogin(credentials);
  }

  /** @private */
  onShowIncognito_() {
    this.browserProxy_.showIncognito();
  }

  /** @private */
  onGetAccounts_() {
    this.browserProxy_.getAccounts().then(result => {
      this.authExtHost_.getAccountsResponse(result);
    });
  }

  /**
   * Loads auth extension.
   * @param {!AuthParams} data Parameters for auth extension.
   * @private
   */
  loadAuthExtension_(data) {
    this.authExtHost_.load(data.authMode, data);
    this.loading_ = true;
    this.isLoginPrimaryAccount_ = data.isLoginPrimaryAccount;
    // <if expr="chromeos_ash">
    // Skip welcome page for reauthentication.
    if (data.email) {
      this.isReauthentication_ = true;
    }
    // </if>
    this.switchToDefaultView_();
  }

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
  }

  /**
   * @param {boolean} loading Indicates whether the page is loading.
   * @param {boolean} verifyingAccount Indicates whether the user account is
   *  being verified.
   * @return {boolean}
   * @private
   */
  isSpinnerActive_(loading, verifyingAccount) {
    return loading || verifyingAccount;
  }

  /**
   * Closes the login dialog.
   * @private
   */
  closeDialog_() {
    this.browserProxy_.dialogClose();
  }

  // <if expr="chromeos_ash">
  /**
   * Navigates to the welcome screen.
   * @private
   */
  goToWelcomeScreen_() {
    this.switchView_(View.WELCOME);
  }

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
      this.switchView_(View.WELCOME);
    } else {
      this.closeDialog_();
    }
  }

  /**
   * @return {string}
   * @private
   */
  getBackButtonIcon_() {
    return isRTL() ? 'cr:chevron-right' : 'cr:chevron-left';
  }

  /**
   * @return {string}
   * @private
   */
  getNextButtonLabel_(currentView, isArcAccountRestrictionsEnabled) {
    if (currentView === View.SIGNIN_BLOCKED_BY_POLICY ||
        currentView === View.SIGNIN_ERROR) {
      return this.i18n('ok');
    }
    if (!isArcAccountRestrictionsEnabled) {
      return this.i18n('ok');
    }
    return this.i18n('nextButtonLabel');
  }

  /**
   * @param {View} currentView Identifier of the view that is being shown.
   * @param {boolean} verifyingAccount Indicates whether the user account is
   *  being verified.
   * @return {boolean}
   * @private
   */
  shouldShowBackButton_(currentView, verifyingAccount) {
    return currentView === View.ADD_ACCOUNT && !verifyingAccount;
  }

  /**
   * @return {boolean}
   * @private
   */
  shouldShowOkButton_() {
    return this.currentView_ === View.WELCOME ||
        this.currentView_ === View.SIGNIN_BLOCKED_BY_POLICY ||
        this.currentView_ === View.SIGNIN_ERROR;
  }

  /**
   * @return {boolean}
   * @private
   */
  shouldShowGaiaButtons_() {
    return this.currentView_ === View.ADD_ACCOUNT;
  }
  // </if>

  /**
   * Navigates to the default view.
   * @private
   */
  switchToDefaultView_() {
    const view = this.getDefaultView_();

    // <if expr="chromeos_ash">
    if (this.isArcAccountRestrictionsEnabled_ &&
        view === View.ARC_ACCOUNT_PICKER) {
      this.shadowRoot.querySelector('arc-account-picker-app')
          .loadAccounts()
          .then(
              accountsFound => {
                this.switchView_(
                    accountsFound ? View.ARC_ACCOUNT_PICKER : View.WELCOME);
              },
              reject => {
                this.switchView_(View.WELCOME);
              });
      return;
    }
    // </if>

    this.switchView_(view);
  }

  /**
   * @return {View}
   * @private
   */
  getDefaultView_() {
    // TODO(https://crbug.com/1155041): simplify this when the file will be
    // split into CrOS and non-CrOS parts.
    // <if expr="not chromeos_ash">
    // On non-ChromeOS always show 'Add account'.
    return View.ADD_ACCOUNT;
    // </if>

    // <if expr="chromeos_ash">
    if (this.isReauthentication_) {
      return View.ADD_ACCOUNT;
    }
    if (this.isArcAccountRestrictionsEnabled_) {
      const options = getAccountAdditionOptionsFromJSON(
          InlineLoginBrowserProxyImpl.getInstance().getDialogArguments());
      if (!!options && options.showArcAvailabilityPicker) {
        return View.ARC_ACCOUNT_PICKER;
      }
    }
    return this.shouldSkipWelcomePage_ ? View.ADD_ACCOUNT : View.WELCOME;
    // </if>
  }

  /**
   * @param {View} id identifier of the view that should be shown.
   * @param {string} enterAnimation enter animation for the new view.
   * @param {string} exitAnimation exit animation for the previous view.
   * @private
   */
  switchView_(id, enterAnimation = 'fade-in', exitAnimation = 'fade-out') {
    this.currentView_ = id;
    /** @type {CrViewManagerElement} */ (this.$.viewManager)
        .switchView(id, enterAnimation, exitAnimation);
    this.dispatchEvent(new CustomEvent('switch-view-notify-for-testing'));
  }

  /**
   * @return {boolean}
   * @private
   */
  isWelcomePageEnabled_() {
    // <if expr="not chromeos_ash">
    return false;
    // </if>
    // <if expr="chromeos_ash">
    return !this.shouldSkipWelcomePage_ && !this.isReauthentication_;
    // </if>
  }

  // <if expr="chromeos_ash">

  /**
   * Shows the sign-in blocked by policy screen if the user account is not
   * allowed to sign-in. Or shows the sign-in error screen if any error occurred
   * during the sign-in flow.
   * @param {{email:string, hostedDomain:string, signinBlockedByPolicy:boolean,
   *  deviceType:string}}
   * data parameters.
   * @private
   */
  signinErrorShowView_(data) {
    this.verifyingAccount_ = false;
    if (data.signinBlockedByPolicy) {
      this.set('email_', data.email);
      this.set('hostedDomain_', data.hostedDomain);
      this.set('deviceType_', data.deviceType);
      this.switchView_(
          View.SIGNIN_BLOCKED_BY_POLICY, 'no-animation', 'no-animation');
    } else {
      this.switchView_(View.SIGNIN_ERROR, 'no-animation', 'no-animation');
    }

    this.setFocusToWebview_();
  }

  /** @private */
  onOkButtonClick_() {
    switch (this.currentView_) {
      case View.WELCOME:
        this.switchView_(View.ADD_ACCOUNT);
        const skipChecked =
            /** @type {WelcomePageAppElement} */ (
                this.shadowRoot.querySelector('welcome-page-app'))
                .isSkipCheckboxChecked();
        this.browserProxy_.skipWelcomePage(skipChecked);
        this.setFocusToWebview_();
        break;
      case View.SIGNIN_BLOCKED_BY_POLICY:
      case View.SIGNIN_ERROR:
        this.closeDialog_();
        break;
    }
  }

  /** @private */
  setFocusToWebview_() {
    this.$.signinFrame.focus();
  }
  // </if>

  /** @param {Object} authExtHost */
  setAuthExtHostForTest(authExtHost) {
    this.authExtHost_ = /** @type {!Authenticator} */ (authExtHost);
    this.addAuthExtHostListeners_();
  }
}

customElements.define(InlineLoginAppElement.is, InlineLoginAppElement);
