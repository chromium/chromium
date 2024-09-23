// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';

import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {CrLitElement, nothing} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ChromeSigninInterceptionParameters, DiceWebSigninInterceptBrowserProxy} from '../dice_web_signin_intercept_browser_proxy.js';
import {DiceWebSigninInterceptBrowserProxyImpl} from '../dice_web_signin_intercept_browser_proxy.js';

import {getCss} from './chrome_signin_app.css.js';
import {getHtml} from './chrome_signin_app.html.js';

export interface ChromeSigninAppElement {
  $: {
    interceptDialog: HTMLElement,
  };
}

const ChromeSigninAppElementBase =
    I18nMixinLit(WebUiListenerMixinLit(CrLitElement));

export class ChromeSigninAppElement extends ChromeSigninAppElementBase {
  static get is() {
    return 'chrome-signin-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      interceptionParameters_: {type: Object},
    };
  }

  protected interceptionParameters_: ChromeSigninInterceptionParameters = {
    title: '',
    subtitle: '',
    fullName: '',
    givenName: '',
    email: '',
    pictureUrl: '',
    managedUserBadge: '',
  };
  private diceWebSigninInterceptBrowserProxy_:
      DiceWebSigninInterceptBrowserProxy =
          DiceWebSigninInterceptBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'interception-chrome-signin-parameters-changed',
        this.setParameters_.bind(this));

    this.diceWebSigninInterceptBrowserProxy_.chromeSigninPageLoaded().then(
        parameters => this.onParametersLoaded_(parameters));
  }

  private setParameters_(parameters: ChromeSigninInterceptionParameters) {
    this.interceptionParameters_ = parameters;
  }

  private async onParametersLoaded_(parameters:
                                        ChromeSigninInterceptionParameters) {
    this.setParameters_(parameters);

    await this.updateComplete;
    const height = this.$.interceptDialog.offsetHeight;
    this.diceWebSigninInterceptBrowserProxy_.initializedWithHeight(height);
  }

  protected onCancel_() {
    this.diceWebSigninInterceptBrowserProxy_.cancel();
  }

  protected onAccept_() {
    this.diceWebSigninInterceptBrowserProxy_.accept();
  }

  protected getAcceptButtonAriaLabel_() {
    if (this.interceptionParameters_.email.length === 0) {
      return nothing;
    }
    return this.i18n(
        'acceptButtonAriaLabel', this.interceptionParameters_.email);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'chrome-signin-app': ChromeSigninAppElement;
  }
}

customElements.define(ChromeSigninAppElement.is, ChromeSigninAppElement);
