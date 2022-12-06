// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './strings.m.js';
import './signin_shared.css.js';

import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './signin_reauth_app.html.js';
import {SigninReauthBrowserProxy, SigninReauthBrowserProxyImpl} from './signin_reauth_browser_proxy.js';

export interface SigninReauthAppElement {
  $: {
    cancelButton: HTMLElement,
    confirmButton: HTMLElement,
    signinReauthTitle: HTMLElement,
  };
}

const SigninReauthAppElementBase = WebUiListenerMixin(PolymerElement);

export class SigninReauthAppElement extends SigninReauthAppElementBase {
  static get is() {
    return 'signin-reauth-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      accountImageSrc_: {
        type: String,
        value() {
          return loadTimeData.getString('accountImageUrl');
        },
      },

      confirmButtonHidden_: {type: Boolean, value: true},

      cancelButtonHidden_: {type: Boolean, value: true},
    };
  }

  private accountImageSrc_: string;
  private confirmButtonHidden_: boolean;
  private cancelButtonHidden_: boolean;
  private signinReauthBrowserProxy_: SigninReauthBrowserProxy =
      SigninReauthBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'reauth-type-determined', this.onReauthTypeDetermined_.bind(this));
    this.signinReauthBrowserProxy_.initialize();
  }

  private onConfirm_(e: Event) {
    this.signinReauthBrowserProxy_.confirm(
        this.getConsentDescription_(),
        this.getConsentConfirmation_(e.composedPath() as HTMLElement[]));
  }

  private onCancel_() {
    this.signinReauthBrowserProxy_.cancel();
  }

  private onReauthTypeDetermined_() {
    this.confirmButtonHidden_ = false;
    this.cancelButtonHidden_ = false;
  }

  /** @return Text of the consent description elements. */
  private getConsentDescription_(): string[] {
    const consentDescription =
        Array.from(this.shadowRoot!.querySelectorAll('[consent-description]'))
            .filter(element => element.clientWidth * element.clientHeight > 0)
            .map(element => element.innerHTML.trim());
    assert(consentDescription);
    return consentDescription;
  }

  /**
   * @param path Path of the click event. Must contain a consent confirmation
   *     element.
   * @return The text of the consent confirmation element.
   */
  private getConsentConfirmation_(path: HTMLElement[]): string {
    for (const element of path) {
      if (element.nodeType !== Node.DOCUMENT_FRAGMENT_NODE &&
          element.hasAttribute('consent-confirmation')) {
        return element.innerHTML.trim();
      }
    }
    assertNotReached('No consent confirmation element found.');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'signin-reauth-app': SigninReauthAppElement;
  }
}

customElements.define(SigninReauthAppElement.is, SigninReauthAppElement);
