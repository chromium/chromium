// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';
import './shared/action_link_style.css.js';
import './shared/animations.css.js';
import './shared/onboarding_background.js';
import './shared/splash_pages_shared.css.js';
import '../strings.m.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {NavigationMixin} from './navigation_mixin.js';
import type {OnboardingBackgroundElement} from './shared/onboarding_background.js';
import {getTemplate} from './signin_view.html.js';
import type {SigninViewProxy} from './signin_view_proxy.js';
import {SigninViewProxyImpl} from './signin_view_proxy.js';
import type {WelcomeBrowserProxy} from './welcome_browser_proxy.js';
import {WelcomeBrowserProxyImpl} from './welcome_browser_proxy.js';

export interface SigninViewElement {
  $: {
    background: OnboardingBackgroundElement,
  };
}

const SigninViewElementBase = NavigationMixin(PolymerElement);

export class SigninViewElement extends SigninViewElementBase {
  static get is() {
    return 'signin-view';
  }

  static get template() {
    return getTemplate();
  }

  private finalized_: boolean = false;
  private welcomeBrowserProxy_: WelcomeBrowserProxy;
  private signinViewProxy_: SigninViewProxy;

  constructor() {
    super();

    this.signinViewProxy_ = SigninViewProxyImpl.getInstance();
    this.welcomeBrowserProxy_ = WelcomeBrowserProxyImpl.getInstance();
  }

  override onRouteEnter() {
    this.finalized_ = false;
    this.signinViewProxy_.recordPageShown();
    this.$.background.play();
  }

  override onRouteExit() {
    if (this.finalized_) {
      return;
    }
    this.finalized_ = true;
    this.signinViewProxy_.recordNavigatedAwayThroughBrowserHistory();
    this.$.background.pause();
  }

  override onRouteUnload() {
    // URL is expected to change when signing in or skipping.
    if (this.finalized_) {
      return;
    }
    this.finalized_ = true;
    this.signinViewProxy_.recordNavigatedAway();
  }

  private onSignInClick_() {
    this.finalized_ = true;
    this.signinViewProxy_.recordSignIn();
    this.welcomeBrowserProxy_.handleActivateSignIn(null);
  }

  private onNoThanksClick_() {
    this.finalized_ = true;
    this.signinViewProxy_.recordSkip();
    this.welcomeBrowserProxy_.handleUserDecline();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'signin-view': SigninViewElement;
  }
}

customElements.define(SigninViewElement.is, SigninViewElement);
