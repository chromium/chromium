// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';
import './shared/action_link_style_css.js';
import './shared/animations_css.js';
import './shared/splash_pages_shared_css.js';
import '../strings.m.js';

import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {NavigationBehavior, NavigationBehaviorInterface, Routes} from './navigation_behavior.js';
import {OnboardingBackgroundElement} from './shared/onboarding_background.js';
import {SigninViewProxy, SigninViewProxyImpl} from './signin_view_proxy.js';
import {WelcomeBrowserProxy, WelcomeBrowserProxyImpl} from './welcome_browser_proxy.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {NavigationBehaviorInterface}
 */
const SigninViewElementBase =
    mixinBehaviors([NavigationBehavior], PolymerElement);

/** @polymer */
export class SigninViewElement extends SigninViewElementBase {
  static get is() {
    return 'signin-view';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  constructor() {
    super();

    /** @private {boolean} */
    this.finalized_ = false;

    /** @private {?WelcomeBrowserProxy} */
    this.welcomeBrowserProxy_ = null;

    /** @private {?SigninViewProxy} */
    this.signinViewProxy_ = null;
  }

  /** @override */
  ready() {
    super.ready();
    this.welcomeBrowserProxy_ = WelcomeBrowserProxyImpl.getInstance();
    this.signinViewProxy_ = SigninViewProxyImpl.getInstance();
  }

  onRouteEnter() {
    this.finalized_ = false;
    this.signinViewProxy_.recordPageShown();
    /** @type {!OnboardingBackgroundElement} */ (this.$.background).play();
  }

  onRouteExit() {
    if (this.finalized_) {
      return;
    }
    this.finalized_ = true;
    this.signinViewProxy_.recordNavigatedAwayThroughBrowserHistory();
    /** @type {!OnboardingBackgroundElement} */ (this.$.background).pause();
  }

  onRouteUnload() {
    // URL is expected to change when signing in or skipping.
    if (this.finalized_) {
      return;
    }
    this.finalized_ = true;
    this.signinViewProxy_.recordNavigatedAway();
  }

  /** private */
  onSignInClick_() {
    this.finalized_ = true;
    this.signinViewProxy_.recordSignIn();
    this.welcomeBrowserProxy_.handleActivateSignIn(null);
  }

  /** @private */
  onNoThanksClick_() {
    this.finalized_ = true;
    this.signinViewProxy_.recordSkip();
    this.welcomeBrowserProxy_.handleUserDecline();
  }
}
customElements.define(SigninViewElement.is, SigninViewElement);
