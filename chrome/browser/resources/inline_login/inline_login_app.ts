// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import type {AuthCompletedCredentials, AuthParams} from 'chrome://chrome-signin/gaia_auth_host/authenticator.js';
import {Authenticator} from 'chrome://chrome-signin/gaia_auth_host/authenticator.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './inline_login_app.css.js';
import {getHtml} from './inline_login_app.html.js';
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
    spinner: HTMLElement,
  };
}

const InlineLoginAppElementBase = WebUiListenerMixinLit(CrLitElement);

export class InlineLoginAppElement extends InlineLoginAppElementBase {
  static get is() {
    return 'inline-login-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * Indicates whether the page is loading.
       */
      loading_: {type: Boolean},

      /**
       * Indicates whether the account is being verified.
       */
      verifyingAccount_: {type: Boolean},

      /**
       * The auth extension host instance.
       */
      authenticator_: {type: Object},
    };
  }

  protected accessor loading_: boolean = true;
  protected accessor verifyingAccount_: boolean = false;
  private accessor authenticator_: Authenticator|null = null;

  /** Whether the login UI is loaded for signing in primary account. */
  private isLoginPrimaryAccount_: boolean = false;

  private browserProxy_: InlineLoginBrowserProxy =
      InlineLoginBrowserProxyImpl.getInstance();

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

  override firstUpdated() {
    this.authenticator_ = new Authenticator(this.$.signinFrame);
    this.addAuthenticatorListeners_();
    this.browserProxy_.initialize();
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

  protected isSpinnerActive_(): boolean {
    return this.loading_ || this.verifyingAccount_;
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
