// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';

import type {AuthCompletedCredentials, AuthParams} from 'chrome://chrome-signin/gaia_auth_host/authenticator.js';
import {Authenticator} from 'chrome://chrome-signin/gaia_auth_host/authenticator.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import type {PaperSpinnerLiteElement} from 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './inline_login_app.html.js';
import type {InlineLoginBrowserProxy} from './inline_login_browser_proxy.js';
import {InlineLoginBrowserProxyImpl} from './inline_login_browser_proxy.js';

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

export interface InlineLoginAppElement {
  $: {
    signinFrame: WebViewElement,
    spinner: PaperSpinnerLiteElement,
  };
}

const InlineLoginAppElementBase = WebUiListenerMixin(PolymerElement);

export class InlineLoginAppElement extends InlineLoginAppElementBase {
  static get is() {
    return 'inline-login-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
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
    };
  }

  private loading_: boolean;
  private verifyingAccount_: boolean;
  private authenticator_: Authenticator|null;

  /** Whether the login UI is loaded for signing in primary account. */
  private isLoginPrimaryAccount_: boolean = false;

  private browserProxy_: InlineLoginBrowserProxy =
      InlineLoginBrowserProxyImpl.getInstance();

  override ready() {
    super.ready();

    this.authenticator_ = new Authenticator(this.$.signinFrame);
    this.addAuthenticatorListeners_();
    this.browserProxy_.initialize();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'load-authenticator',
        (data: AuthParams) => this.loadAuthenticator_(data));
    this.addWebUiListener(
        'send-lst-fetch-results',
        (arg: string) => this.sendLstFetchResults_(arg));
    this.addWebUiListener('close-dialog', () => this.closeDialog_());
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
  }

  private onDropLink_(e: CustomEvent<string>) {
    // Navigate to the dropped link.
    window.location.href = e.detail;
  }

  private onNewWindow_(e: CustomEvent<NewWindowProperties>) {
    window.open(e.detail.targetUrl, '_blank');
    e.detail.window.discard();
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
    this.browserProxy_.completeLogin(credentials);
  }

  private onShowIncognito_() {
    this.browserProxy_.showIncognito();
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
    this.dispatchEvent(new CustomEvent('switch-view-notify-for-testing'));
  }

  /**
   * Sends a message 'lstFetchResults'. This is a specific message sent when
   * the inline signin is loaded with reason kFetchLstOnly. Handlers of
   * this message would expect a single argument a base::Dictionary value that
   * contains the values fetched from the gaia sign in endpoint.
   * @param arg The string representation of the json data returned by
   *    the sign in dialog after it has finished the sign in process.
   */
  private sendLstFetchResults_(arg: string) {
    this.browserProxy_.lstFetchResults(arg);
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
