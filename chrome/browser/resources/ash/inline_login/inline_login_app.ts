// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/ash/common/cr_elements/cr_view_manager/cr_view_manager.js';
import 'chrome://chrome-signin/arc_account_picker/arc_account_picker_app.js';
import './gaia_action_buttons/gaia_action_buttons.js';
import './signin_blocked_by_policy_page.js';
import './signin_error_page.js';
import './welcome_page_app.js';
import './strings.m.js';

import {getAccountAdditionOptionsFromJSON} from 'chrome://chrome-signin/arc_account_picker/arc_util.js';
import type {AuthCompletedCredentials, AuthParams} from 'chrome://chrome-signin/gaia_auth_host/authenticator.js';
import {Authenticator} from 'chrome://chrome-signin/gaia_auth_host/authenticator.js';
import type {CrViewManagerElement} from 'chrome://resources/ash/common/cr_elements/cr_view_manager/cr_view_manager.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isRTL} from 'chrome://resources/js/util.js';
import type {PaperSpinnerLiteElement} from 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './inline_login_app.html.js';
import type {InlineLoginBrowserProxy} from './inline_login_browser_proxy.js';
import {InlineLoginBrowserProxyImpl} from './inline_login_browser_proxy.js';

/**
 * @fileoverview Inline login WebUI in various signin flows for ChromeOS and
 * Chrome desktop (Windows only).
 */

export enum View {
  ADD_ACCOUNT = 'addAccount',
  ARC_ACCOUNT_PICKER = 'arcAccountPicker',
  SIGNIN_BLOCKED_BY_POLICY = 'signinBlockedByPolicy',
  SIGNIN_ERROR = 'signinError',
  WELCOME = 'welcome',
}

interface NewWindowProperties {
  targetUrl: string;
  window: {
    discard(): void,
  };
}

interface WebViewElement extends HTMLElement {
  canGoBack(): boolean;
  back(): void;
}

interface SigninErrorPageData {
  email: string;
  hostedDomain: string;
  signinBlockedByPolicy: boolean;
  deviceType: string;
}

export interface InlineLoginAppElement {
  $: {
    signinFrame: WebViewElement,
    spinner: PaperSpinnerLiteElement,
    viewManager: CrViewManagerElement,
  };
}

const InlineLoginAppElementBase = WebUiListenerMixin(I18nMixin(PolymerElement));

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
       */
      loading_: {
        type: Boolean,
        value: true,
      },

      /**
       * Indicates whether the account is being verified.
       */
      verifyingAccount_: {
        type: Boolean,
        value: false,
      },

      /**
       * The auth extension host instance.
       */
      authenticator_: {
        type: Object,
        value: null,
      },

      /*
       * True if welcome page should not be shown.
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
       */
      isReauthentication_: {
        type: Boolean,
        value: false,
      },

      /*
       * True if the account should be available in ARC++ after addition.
       */
      isAvailableInArc_: {
        type: Boolean,
        value: false,
      },

      /**
       * User's email used in the sign-in flow.
       */
      email_: {type: String, value: ''},

      /**
       * Hosted domain of the user's email used in the sign-in flow.
       */
      hostedDomain_: {type: String, value: ''},

      /**
       * Whether secondary account sign-ins are allowed.
       */
      isSecondaryGoogleAccountSigninAllowed_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('secondaryGoogleAccountSigninAllowed');
        },
      },

      /**
       * Id of the screen that is currently displayed.
       */
      currentView_: {
        type: String,
        value: '',
      },
    };
  }

  private loading_: boolean;
  private verifyingAccount_: boolean;
  private authenticator_: Authenticator|null;

  private shouldSkipWelcomePage_: boolean;
  private isArcAccountRestrictionsEnabled_: boolean;
  private isReauthentication_: boolean;
  private isAvailableInArc_: boolean;
  private email_: string;
  private hostedDomain_: string;
  private isSecondaryGoogleAccountSigninAllowed_: boolean;

  private currentView_: View;

  /** Whether the login UI is loaded for signing in primary account. */
  private isLoginPrimaryAccount_: boolean = false;

  private browserProxy_: InlineLoginBrowserProxy =
      InlineLoginBrowserProxyImpl.getInstance();

  override ready() {
    super.ready();

    if (!this.isSecondaryGoogleAccountSigninAllowed_) {
      // This can happen only if the user opened chrome://chrome-signin manually
      // in the browser. Normally (in the account addition dialog) this will be
      // handled earlier and a special error screen will be shown.
      console.warn(
          'SecondaryGoogleAccountSigninAllowed is set to false - aborting.');
      return;
    }

    this.authenticator_ = new Authenticator(this.$.signinFrame);
    this.addAuthenticatorListeners_();
    this.browserProxy_.initialize();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'load-authenticator',
        (data: AuthParams) => this.loadAuthenticator_(data));
    this.addWebUiListener('close-dialog', () => this.closeDialog_());
    this.addWebUiListener(
        'show-signin-error-page',
        (data: SigninErrorPageData) => this.signinErrorShowView_(data));
  }

  private addAuthenticatorListeners_() {
    assert(this.authenticator_);
    this.authenticator_.addEventListener(
        'dropLink', e => this.onDropLink_(e as CustomEvent<string>));
    this.authenticator_.addEventListener(
        'newWindow',
        e => this.onNewWindow_(e as CustomEvent<NewWindowProperties>));
    this.authenticator_.addEventListener('ready', () => this.onAuthReady_());
    this.authenticator_.addEventListener(
        'resize', e => this.onResize_(e as CustomEvent<string>));
    this.authenticator_.addEventListener(
        'authCompleted',
        e => this.onAuthCompleted_(e as CustomEvent<AuthCompletedCredentials>));
    this.authenticator_.addEventListener(
        'showIncognito', () => this.onShowIncognito_());
    this.authenticator_.addEventListener(
        'getAccounts', () => this.onGetAccounts_());
    this.authenticator_.addEventListener(
        'getDeviceId', () => this.onGetDeviceId_());
  }

  private onDropLink_(e: CustomEvent<string>) {
    // Navigate to the dropped link.
    window.location.href = e.detail;
  }

  private onNewWindow_(e: CustomEvent<NewWindowProperties>) {
    window.open(e.detail.targetUrl, '_blank');
    e.detail.window.discard();
    // On Chrome OS this dialog is always-on-top, so we have to close it if
    // user opens a link in a new window.
    this.closeDialog_();
  }

  private onAuthReady_() {
    this.loading_ = false;
    if (this.isLoginPrimaryAccount_) {
      this.browserProxy_.recordAction('Signin_SigninPage_Shown');
    }
    this.browserProxy_.authenticatorReady();
  }

  private onResize_(e: CustomEvent<string>) {
    this.browserProxy_.switchToFullTab(e.detail);
  }

  private onAuthCompleted_(e: CustomEvent<AuthCompletedCredentials>) {
    this.verifyingAccount_ = true;
    const credentials = e.detail;

    if (this.isArcAccountRestrictionsEnabled_ && !this.isReauthentication_) {
      credentials.isAvailableInArc = this.isAvailableInArc_;
    }

    this.browserProxy_.completeLogin(credentials);
  }

  private onShowIncognito_() {
    this.browserProxy_.showIncognito();
  }

  private onGetAccounts_() {
    this.browserProxy_.getAccounts().then(result => {
      assert(this.authenticator_);
      this.authenticator_.getAccountsResponse(result);
    });
  }

  private onGetDeviceId_() {
    this.browserProxy_.getDeviceId().then(deviceId => {
      assert(this.authenticator_);
      this.authenticator_.getDeviceIdResponse(deviceId);
    });
  }

  /**
   * Loads auth extension.
   * @param data Parameters for auth extension.
   */
  private loadAuthenticator_(data: AuthParams) {
    assert(this.authenticator_);
    this.authenticator_.load(data.authMode, data);
    this.loading_ = true;
    this.isLoginPrimaryAccount_ = data.isLoginPrimaryAccount;
    // Skip welcome page for reauthentication.
    if (data.email) {
      this.isReauthentication_ = true;
    }
    this.switchToDefaultView_();
  }

  /**
   * @param loading Indicates whether the page is loading.
   * @param verifyingAccount Indicates whether the user account is being
   *     verified.
   */
  private isSpinnerActive_(loading: boolean, verifyingAccount: boolean):
      boolean {
    return loading || verifyingAccount;
  }

  /**
   * Closes the login dialog.
   */
  private closeDialog_() {
    this.browserProxy_.dialogClose();
  }

  /**
   * Navigates to the welcome screen.
   */
  private goToWelcomeScreen_() {
    this.switchView_(View.WELCOME);
  }

  /**
   * Navigates back in the web view if possible. Otherwise closes the dialog.
   */
  private handleGoBack_() {
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

  private getBackButtonIcon_(): string {
    return isRTL() ? 'cr:chevron-right' : 'cr:chevron-left';
  }

  private getNextButtonLabel_(
      currentView: View, isArcAccountRestrictionsEnabled: boolean): string {
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
   * @param currentView Identifier of the view that is being shown.
   * @param verifyingAccount Indicates whether the user account is being
   *     verified.
   */
  private shouldShowBackButton_(currentView: View, verifyingAccount: boolean):
      boolean {
    return currentView === View.ADD_ACCOUNT && !verifyingAccount;
  }

  private shouldShowOkButton_(): boolean {
    return this.currentView_ === View.WELCOME ||
        this.currentView_ === View.SIGNIN_BLOCKED_BY_POLICY ||
        this.currentView_ === View.SIGNIN_ERROR;
  }

  private shouldShowGaiaButtons_(): boolean {
    return this.currentView_ === View.ADD_ACCOUNT;
  }

  /**
   * Navigates to the default view.
   */
  private switchToDefaultView_() {
    const view = this.getDefaultView_();

    if (this.isArcAccountRestrictionsEnabled_ &&
        view === View.ARC_ACCOUNT_PICKER) {
      const arcAccountPickerApp =
          this.shadowRoot!.querySelector('arc-account-picker-app');
      assert(arcAccountPickerApp);
      arcAccountPickerApp.loadAccounts().then(
          (accountsFound: boolean) => {
            this.switchView_(
                accountsFound ? View.ARC_ACCOUNT_PICKER : View.WELCOME);
          },
          (_error: Error) => {
            this.switchView_(View.WELCOME);
          });
      return;
    }

    this.switchView_(view);
  }

  private getDefaultView_(): View {
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
  }

  /**
   * @param id identifier of the view that should be shown.
   * @param enterAnimation enter animation for the new view.
   * @param exitAnimation exit animation for the previous view.
   */
  private switchView_(
      id: View, enterAnimation: string = 'fade-in',
      exitAnimation: string = 'fade-out') {
    this.currentView_ = id;
    this.$.viewManager.switchView(id, enterAnimation, exitAnimation);
    this.dispatchEvent(new CustomEvent('switch-view-notify-for-testing'));
  }

  private isWelcomePageEnabled_(): boolean {
    return !this.shouldSkipWelcomePage_ && !this.isReauthentication_;
  }

  /**
   * Shows the sign-in blocked by policy screen if the user account is not
   * allowed to sign-in. Or shows the sign-in error screen if any error occurred
   * during the sign-in flow.
   */
  private signinErrorShowView_(data: SigninErrorPageData) {
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

  private onOkButtonClick_() {
    switch (this.currentView_) {
      case View.WELCOME:
        this.switchView_(View.ADD_ACCOUNT);
        const welcomePageApp =
            this.shadowRoot!.querySelector('welcome-page-app');
        assert(welcomePageApp);
        const skipChecked = welcomePageApp.isSkipCheckboxChecked();
        this.browserProxy_.skipWelcomePage(skipChecked);
        this.setFocusToWebview_();
        break;
      case View.SIGNIN_BLOCKED_BY_POLICY:
      case View.SIGNIN_ERROR:
        this.closeDialog_();
        break;
    }
  }

  private setFocusToWebview_() {
    this.$.signinFrame.focus();
  }

  setAuthenticatorForTest(authenticator: Authenticator) {
    this.authenticator_ = authenticator;
    this.addAuthenticatorListeners_();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'inline-login-app': InlineLoginAppElement;
  }
}

customElements.define(InlineLoginAppElement.is, InlineLoginAppElement);
