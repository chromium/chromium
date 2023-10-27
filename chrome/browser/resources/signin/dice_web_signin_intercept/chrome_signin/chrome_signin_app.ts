// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';
import '../signin_shared.css.js';
import '../signin_vars.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ChromeSigninInterceptionParameters, DiceWebSigninInterceptBrowserProxy, DiceWebSigninInterceptBrowserProxyImpl} from '../dice_web_signin_intercept_browser_proxy.js';

import {getTemplate} from './chrome_signin_app.html.js';

const ChromeSigninAppElementBase =
    I18nMixin(WebUiListenerMixin(PolymerElement));

export class ChromeSigninAppElement extends ChromeSigninAppElementBase {
  static get is() {
    return 'chrome-signin-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      interceptionParameters_: {
        type: Object,
        value: null,
      },
    };
  }

  private interceptionParameters_: ChromeSigninInterceptionParameters;
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

  private onParametersLoaded_(parameters: ChromeSigninInterceptionParameters) {
    this.setParameters_(parameters);

    afterNextRender(this, () => {
      const height =
          this.shadowRoot!.querySelector<HTMLElement>(
                              '#interceptDialog')!.offsetHeight;
      this.diceWebSigninInterceptBrowserProxy_.initializedWithHeight(height);
    });
  }

  private onCancel_() {
    this.diceWebSigninInterceptBrowserProxy_.cancel();
  }

  private onAccept_() {
    this.diceWebSigninInterceptBrowserProxy_.accept();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'chrome-signin-app': ChromeSigninAppElement;
  }
}

customElements.define(ChromeSigninAppElement.is, ChromeSigninAppElement);
