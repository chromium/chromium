// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import './strings.m.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './dice_web_signin_intercept_app.css.js';
import {getHtml} from './dice_web_signin_intercept_app.html.js';
import type {DiceWebSigninInterceptBrowserProxy, InterceptionParameters} from './dice_web_signin_intercept_browser_proxy.js';
import {DiceWebSigninInterceptBrowserProxyImpl} from './dice_web_signin_intercept_browser_proxy.js';

const DiceWebSigninInterceptAppElementBase =
    WebUiListenerMixinLit(CrLitElement);

export interface DiceWebSigninInterceptAppElement {
  $: {
    interceptDialog: HTMLElement,
    cancelButton: CrButtonElement,
    acceptButton: CrButtonElement,
  };
}

export class DiceWebSigninInterceptAppElement extends
    DiceWebSigninInterceptAppElementBase {
  static get is() {
    return 'dice-web-signin-intercept-app';
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
      acceptButtonClicked_: {type: Boolean},
    };
  }

  protected interceptionParameters_: InterceptionParameters = {
    headerText: '',
    bodyTitle: '',
    bodyText: '',
    confirmButtonLabel: '',
    cancelButtonLabel: '',
    managedDisclaimerText: '',
    headerTextColor: '',
    interceptedProfileColor: '',
    primaryProfileColor: '',
    interceptedAccount: {pictureUrl: '', avatarBadge: ''},
    primaryAccount: {pictureUrl: '', avatarBadge: ''},
    useV2Design: false,
    showManagedDisclaimer: false,
  };
  protected acceptButtonClicked_: boolean = false;
  private diceWebSigninInterceptBrowserProxy_:
      DiceWebSigninInterceptBrowserProxy =
          DiceWebSigninInterceptBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'interception-parameters-changed',
        this.handleParametersChanged_.bind(this));
    this.diceWebSigninInterceptBrowserProxy_.pageLoaded().then(
        parameters => this.onPageLoaded_(parameters));
  }

  private async onPageLoaded_(parameters: InterceptionParameters) {
    this.handleParametersChanged_(parameters);
    await this.updateComplete;
    const height = this.$.interceptDialog.offsetHeight;
    this.diceWebSigninInterceptBrowserProxy_.initializedWithHeight(height);
  }

  protected onAccept_() {
    this.acceptButtonClicked_ = true;
    this.diceWebSigninInterceptBrowserProxy_.accept();
  }

  protected onCancel_() {
    this.diceWebSigninInterceptBrowserProxy_.cancel();
  }

  /** Called when the interception parameters are updated. */
  private handleParametersChanged_(parameters: InterceptionParameters) {
    this.interceptionParameters_ = parameters;
    this.style.setProperty(
        '--intercepted-profile-color', parameters.interceptedProfileColor);
    this.style.setProperty(
        '--primary-profile-color', parameters.primaryProfileColor);
    this.style.setProperty('--header-text-color', parameters.headerTextColor);
  }

  protected sanitizeInnerHtml_(text: string): TrustedHTML {
    return sanitizeInnerHtml(text);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'dice-web-signin-intercept-app': DiceWebSigninInterceptAppElement;
  }
}

customElements.define(
    DiceWebSigninInterceptAppElement.is, DiceWebSigninInterceptAppElement);
