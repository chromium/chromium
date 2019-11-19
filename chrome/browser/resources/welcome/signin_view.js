// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';
import './shared/action_link_style_css.js';
import './shared/animations_css.js';
import './shared/onboarding_background.js';
import './shared/splash_pages_shared_css.js';
import '../strings.m.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {navigateTo, navigateToNextStep, NavigationBehavior, Routes} from './navigation_behavior.js';
import {SigninViewProxy, SigninViewProxyImpl} from './signin_view_proxy.js';
import {WelcomeBrowserProxy, WelcomeBrowserProxyImpl} from './welcome_browser_proxy.js';

Polymer({
  is: 'signin-view',

  _template: html`{__html_template__}`,

  behaviors: [NavigationBehavior],

  /** @private {boolean} */
  finalized_: false,

  /** @private {?WelcomeBrowserProxy} */
  welcomeBrowserProxy_: null,

  /** @private {?SigninViewProxy} */
  signinViewProxy_: null,

  /** @override */
  ready: function() {
    this.welcomeBrowserProxy_ = WelcomeBrowserProxyImpl.getInstance();
    this.signinViewProxy_ = SigninViewProxyImpl.getInstance();
  },

  onRouteEnter: function() {
    this.finalized_ = false;
    this.signinViewProxy_.recordPageShown();
  },

  onRouteExit: function() {
    if (this.finalized_) {
      return;
    }
    this.finalized_ = true;
    this.signinViewProxy_.recordNavigatedAwayThroughBrowserHistory();
  },

  onRouteUnload: function() {
    // URL is expected to change when signing in or skipping.
    if (this.finalized_) {
      return;
    }
    this.finalized_ = true;
    this.signinViewProxy_.recordNavigatedAway();
  },

  /** private */
  onSignInClick_: function() {
    this.finalized_ = true;
    this.signinViewProxy_.recordSignIn();
    this.welcomeBrowserProxy_.handleActivateSignIn(null);
  },

  /** @private */
  onNoThanksClick_: function() {
    this.finalized_ = true;
    this.signinViewProxy_.recordSkip();
    this.welcomeBrowserProxy_.handleUserDecline();
  }
});
