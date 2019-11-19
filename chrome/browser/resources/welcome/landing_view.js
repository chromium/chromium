// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';
import './shared/action_link_style_css.js';
import './shared/onboarding_background.js';
import './shared/splash_pages_shared_css.js';
import '../strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LandingViewProxy, LandingViewProxyImpl} from './landing_view_proxy.js';
import {navigateTo, navigateToNextStep, NavigationBehavior, Routes} from './navigation_behavior.js';
import {WelcomeBrowserProxyImpl} from './welcome_browser_proxy.js';

Polymer({
  is: 'landing-view',

  _template: html`{__html_template__}`,

  behaviors: [NavigationBehavior],

  properties: {
    /** @private */
    signinAllowed_: {
      type: Boolean,
      value: () => loadTimeData.getBoolean('signinAllowed'),
    }
  },

  /** @private {?LandingViewProxy} */
  landingViewProxy_: null,

  /** @private {boolean} */
  finalized_: false,

  /** @override */
  ready() {
    this.landingViewProxy_ = LandingViewProxyImpl.getInstance();
  },

  onRouteEnter: function() {
    this.finalized_ = false;
    this.landingViewProxy_.recordPageShown();
  },

  onRouteUnload: function() {
    // Clicking on 'Returning user' will change the URL.
    if (this.finalized_) {
      return;
    }
    this.finalized_ = true;
    this.landingViewProxy_.recordNavigatedAway();
  },

  /** @private */
  onExistingUserClick_: function() {
    this.finalized_ = true;
    this.landingViewProxy_.recordExistingUser();
    if (this.signinAllowed_) {
      WelcomeBrowserProxyImpl.getInstance().handleActivateSignIn(
        'chrome://welcome/returning-user');
    } else {
      navigateTo(Routes.RETURNING_USER, 1);
    }
  },

  /** @private */
  onNewUserClick_: function() {
    this.finalized_ = true;
    this.landingViewProxy_.recordNewUser();
    navigateTo(Routes.NEW_USER, 1);
  }
});
