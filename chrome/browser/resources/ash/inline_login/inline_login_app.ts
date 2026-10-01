// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/ash/common/cr_elements/cr_view_manager/cr_view_manager.js';
import 'chrome://chrome-signin/gaia_action_buttons/gaia_action_buttons.js';
import './signin_blocked_by_policy_page.js';
import './signin_error_page.js';
import './welcome_page_app.js';
import '/strings.m.js';

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
      viewEnum: {
        type: Object,
        value: View,
      },

      /**
       * Indicates whether the page is loading.
       */
      loading: {
        type: Boolean,
        value: true,
      },

      /**
       * Indicates whether the account is being verified.
       */
      verifyingAccount: {
        type: Boolean,
        value: false,
      },

      /**
       * The auth extension host instance.
       */
      authenticator: {
        type: Object,
        value: null,
      },

      /*
       * True if welcome page should not be shown.
       */
      shouldSkipWelcomePage: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('shouldSkipWelcomePage');
        },
        readOnly: true,
      },

      /*
       * True if the dialog is open for reauthentication.
       */
      isReauthentication: {
        type: Boolean,
        value: false,
      },

      /**
       * User's email used in the sign-in flow.
       */
      email: {type: String, value: ''},

      /**
       * Hosted domain of the user's email used in the sign-in flow.
       */
      hostedDomain: {type: String, value: ''},

      /**
       * Device type used in the sign-in flow.
       */
      deviceType: {type: String, value: ''},

      /**
       * Whether secondary account sign-ins are allowed.
       */
      isSecondaryGoogleAccountSigninAllowed: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('secondaryGoogleAccountSigninAllowed');
        },
      },

      /**
       * Id of the screen that is currently displayed.
       */
      currentView: {
        type: String,
        value: '',
      },
    };
  }

  declare protected viewEnum: typeof View;
  declare protected loading: boolean;
  declare protected verifyingAccount: boolean;
  declare protected authenticator: Authenticator|null;

  declare protected shouldSkipWelcomePage: boolean;
  declare protected isReauthentication: boolean;
  declare protected email: string;
  declare protected hostedDomain: string;
  declare protected deviceType: string;
  declare protected isSecondaryGoogleAccountSigninAllowed: boolean;

  declare protected currentView: View;

  /** Whether the login UI is loaded for signing in primary account. */
  private isLoginPrimaryAccount_: boolean = false;

  private browserProxy_: InlineLoginBrowserProxy =
      InlineLoginBrowserProxyImpl.getInstance();

  override ready() {
    super.ready();

    if (!this.isSecondaryGoogleAccountSigninAllowed) {
      // This can happen only if the user opened chrome://chrome-signin manually
      // in the browser. Normally (in the account addition dialog) this will be
      // handled earlier and a special error screen will be shown.
      console.warn(
          'SecondaryGoogleAccountSigninAllowed is set to false - aborting.');
      return;
    }

    this.authenticator = new Authenticator(this.$.signinFrame);
    this.addAuthenticatorListeners_();
    this.browserProxy_.initialize();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'load-authenticator',
        (data: AuthParams) => this.loadAuthenticator_(data));
    this.addWebUiListener('close-dialog', () => this.closeDialog());
    this.addWebUiListener(
        'show-signin-error-page',
        (data: SigninErrorPageData) => this.signinErrorShowView_(data));
  }

  private addAuthenticatorListeners_() {
    assert(this.authenticator);
    this.authenticator.addEventListener(
        'dropLink', e => this.onDropLink_(e as CustomEvent<string>));
    this.authenticator.addEventListener(
        'newWindow',
        e => this.onNewWindow_(e as CustomEvent<NewWindowProperties>));
    this.authenticator.addEventListener('ready', () => this.onAuthReady_());
    this.authenticator.addEventListener(
        'resize', e => this.onResize_(e as CustomEvent<string>));
    this.authenticator.addEventListener(
        'authCompleted',
        e => this.onAuthCompleted_(e as CustomEvent<AuthCompletedCredentials>));
    this.authenticator.addEventListener(
        'showIncognito', () => this.onShowIncognito_());
    this.authenticator.addEventListener(
        'getAccounts', () => this.onGetAccounts_());
    this.authenticator.addEventListener(
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
    this.closeDialog();
  }

  private onAuthReady_() {
    this.loading = false;
    if (this.isLoginPrimaryAccount_) {
      this.browserProxy_.recordAction('Signin_SigninPage_Shown');
    }
    this.browserProxy_.authenticatorReady();
  }

  private onResize_(e: CustomEvent<string>) {
    this.browserProxy_.switchToFullTab(e.detail);
  }

  private onAuthCompleted_(e: CustomEvent<AuthCompletedCredentials>) {
    this.verifyingAccount = true;
    const credentials = e.detail;
    this.browserProxy_.completeLogin(credentials);
  }

  private onShowIncognito_() {
    this.browserProxy_.showIncognito();
  }

  private onGetAccounts_() {
    this.browserProxy_.getAccounts().then(result => {
      assert(this.authenticator);
      this.authenticator.getAccountsResponse(result);
    });
  }

  private onGetDeviceId_() {
    this.browserProxy_.getDeviceId().then(deviceId => {
      assert(this.authenticator);
      this.authenticator.getDeviceIdResponse(deviceId);
    });
  }

  /**
   * Loads auth extension.
   * @param data Parameters for auth extension.
   */
  private loadAuthenticator_(data: AuthParams) {
    assert(this.authenticator);
    this.authenticator.load(data.authMode, data);
    this.loading = true;
    this.isLoginPrimaryAccount_ = data.isLoginPrimaryAccount;
    // Skip welcome page for reauthentication.
    if (data.email) {
      this.isReauthentication = true;
    }
    this.switchToDefaultView_();
  }

  /**
   * @param loading Indicates whether the page is loading.
   * @param verifyingAccount Indicates whether the user account is being
   *     verified.
   */
  protected isSpinnerActive(loading: boolean, verifyingAccount: boolean):
      boolean {
    return loading || verifyingAccount;
  }

  /**
   * Closes the login dialog.
   */
  protected closeDialog() {
    this.browserProxy_.dialogClose();
  }

  /**
   * Navigates back in the web view if possible. Otherwise closes the dialog.
   */
  protected handleGoBack() {
    if (this.$.signinFrame.canGoBack()) {
      this.$.signinFrame.back();
      this.$.signinFrame.focus();
    } else if (this.isWelcomePageEnabled_()) {
      // Allow user go back to the welcome page, if it's enabled.
      this.switchView_(View.WELCOME);
    } else {
      this.closeDialog();
    }
  }

  protected getBackButtonIcon(): string {
    return isRTL() ? 'cr:chevron-right' : 'cr:chevron-left';
  }

  /**
   * @param currentView Identifier of the view that is being shown.
   * @param verifyingAccount Indicates whether the user account is being
   *     verified.
   */
  protected shouldShowBackButton(currentView: View, verifyingAccount: boolean):
      boolean {
    return currentView === View.ADD_ACCOUNT && !verifyingAccount;
  }

  protected shouldShowOkButton(): boolean {
    return this.currentView === View.WELCOME ||
        this.currentView === View.SIGNIN_BLOCKED_BY_POLICY ||
        this.currentView === View.SIGNIN_ERROR;
  }

  protected shouldShowGaiaButtons(): boolean {
    return this.currentView === View.ADD_ACCOUNT;
  }

  /**
   * Navigates to the default view.
   */
  private switchToDefaultView_() {
    const view = this.getDefaultView_();
    this.switchView_(view);
  }

  private getDefaultView_(): View {
    if (this.isReauthentication) {
      return View.ADD_ACCOUNT;
    }
    return this.shouldSkipWelcomePage ? View.ADD_ACCOUNT : View.WELCOME;
  }

  /**
   * @param id identifier of the view that should be shown.
   * @param enterAnimation enter animation for the new view.
   * @param exitAnimation exit animation for the previous view.
   */
  private switchView_(
      id: View, enterAnimation: string = 'fade-in',
      exitAnimation: string = 'fade-out') {
    this.currentView = id;
    this.$.viewManager.switchView(id, enterAnimation, exitAnimation);
    this.dispatchEvent(new CustomEvent('switch-view-notify-for-testing'));
  }

  private isWelcomePageEnabled_(): boolean {
    return !this.shouldSkipWelcomePage && !this.isReauthentication;
  }

  /**
   * Shows the sign-in blocked by policy screen if the user account is not
   * allowed to sign-in. Or shows the sign-in error screen if any error occurred
   * during the sign-in flow.
   */
  private signinErrorShowView_(data: SigninErrorPageData) {
    this.verifyingAccount = false;
    if (data.signinBlockedByPolicy) {
      this.set('email', data.email);
      this.set('hostedDomain', data.hostedDomain);
      this.set('deviceType', data.deviceType);
      this.switchView_(
          View.SIGNIN_BLOCKED_BY_POLICY, 'no-animation', 'no-animation');
    } else {
      this.switchView_(View.SIGNIN_ERROR, 'no-animation', 'no-animation');
    }

    this.setFocusToWebview();
  }

  protected onOkButtonClick() {
    switch (this.currentView) {
      case View.WELCOME:
        this.switchView_(View.ADD_ACCOUNT);
        const welcomePageApp =
            this.shadowRoot!.querySelector('welcome-page-app');
        assert(welcomePageApp);
        const skipChecked = welcomePageApp.isSkipCheckboxChecked();
        this.browserProxy_.skipWelcomePage(skipChecked);
        this.setFocusToWebview();
        break;
      case View.SIGNIN_BLOCKED_BY_POLICY:
      case View.SIGNIN_ERROR:
        this.closeDialog();
        break;
      default:
        break;
    }
  }

  protected setFocusToWebview() {
    this.$.signinFrame.focus();
  }

  setAuthenticatorForTest(authenticator: Authenticator) {
    this.authenticator = authenticator;
    this.addAuthenticatorListeners_();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'inline-login-app': InlineLoginAppElement;
  }
}

customElements.define(InlineLoginAppElement.is, InlineLoginAppElement);
