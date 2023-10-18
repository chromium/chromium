// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DiceWebSigninInterceptBrowserProxy, DiceWebSigninInterceptBrowserProxyImpl} from '../dice_web_signin_intercept_browser_proxy.js';

import {getTemplate} from './chrome_signin_app.html.js';

export class ChromeSigninAppElement extends PolymerElement {
  static get is() {
    return 'chrome-signin-app';
  }

  static get template() {
    return getTemplate();
  }

  private diceWebSigninInterceptBrowserProxy_:
      DiceWebSigninInterceptBrowserProxy =
          DiceWebSigninInterceptBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    const height =
        this.shadowRoot!.querySelector<HTMLElement>(
                            '#interceptDialog')!.offsetHeight;
    this.diceWebSigninInterceptBrowserProxy_.initializedWithHeight(height);
  }

  private onCancel_() {
    this.diceWebSigninInterceptBrowserProxy_.cancel();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'chrome-signin-app': ChromeSigninAppElement;
  }
}

customElements.define(ChromeSigninAppElement.is, ChromeSigninAppElement);
