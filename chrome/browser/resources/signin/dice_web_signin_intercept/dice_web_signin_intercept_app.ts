// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import 'chrome://resources/cr_elements/icons.html.js';
import './signin_shared.css.js';
import './signin_vars.css.js';
import './strings.m.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './dice_web_signin_intercept_app.html.js';
import {DiceWebSigninInterceptBrowserProxy, DiceWebSigninInterceptBrowserProxyImpl, InterceptionParameters} from './dice_web_signin_intercept_browser_proxy.js';

const DiceWebSigninInterceptAppElementBase = WebUiListenerMixin(PolymerElement);

export interface DiceWebSigninInterceptAppElement {
  $: {
    cancelButton: CrButtonElement,
    acceptButton: CrButtonElement,
  };
}

export class DiceWebSigninInterceptAppElement extends
    DiceWebSigninInterceptAppElementBase {
  static get is() {
    return 'dice-web-signin-intercept-app';
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

      acceptButtonClicked_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private interceptionParameters_: InterceptionParameters;
  private acceptButtonClicked_: boolean;
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

  private onPageLoaded_(parameters: InterceptionParameters) {
    this.handleParametersChanged_(parameters);
    afterNextRender(this, () => {
      const height =
          this.shadowRoot!.querySelector<HTMLElement>(
                              '#interceptDialog')!.offsetHeight;
      this.diceWebSigninInterceptBrowserProxy_.initializedWithHeight(height);
    });
  }

  private onAccept_() {
    this.acceptButtonClicked_ = true;
    this.diceWebSigninInterceptBrowserProxy_.accept();
  }

  private onCancel_() {
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
    this.notifyPath('interceptionParameters_.interceptedAccount.isManaged');
    this.notifyPath('interceptionParameters_.primaryAccount.isManaged');
  }

  private sanitizeInnerHtml_(text: string): TrustedHTML {
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
