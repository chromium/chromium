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
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LandingViewProxy, LandingViewProxyImpl} from './landing_view_proxy.js';
import {navigateTo, NavigationBehavior, Routes} from './navigation_behavior.js';
import {OnboardingBackgroundElement} from './shared/onboarding_background.js';
import {WelcomeBrowserProxyImpl} from './welcome_browser_proxy.js';

const LandingViewElementBase =
    mixinBehaviors([NavigationBehavior], PolymerElement);

/** @polymer */
export class LandingViewElement extends LandingViewElementBase {
  static get is() {
    return 'landing-view';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      signinAllowed_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('signinAllowed'),
      }
    };
  }

  private landingViewProxy_: LandingViewProxy|null = null;
  private finalized_: boolean = false;

  ready() {
    super.ready();
    this.landingViewProxy_ = LandingViewProxyImpl.getInstance();
  }

  onRouteEnter() {
    this.finalized_ = false;
    this.landingViewProxy_.recordPageShown();
    (this.$.background as OnboardingBackgroundElement).play();
  }

  onRouteExit() {
    (this.$.background as OnboardingBackgroundElement).pause();
  }

  onRouteUnload() {
    // Clicking on 'Returning user' will change the URL.
    if (this.finalized_) {
      return;
    }
    this.finalized_ = true;
    this.landingViewProxy_.recordNavigatedAway();
  }

  private onExistingUserClick_() {
    this.finalized_ = true;
    this.landingViewProxy_.recordExistingUser();
    if (this.signinAllowed_) {
      WelcomeBrowserProxyImpl.getInstance().handleActivateSignIn(
          'chrome://welcome/returning-user');
    } else {
      navigateTo(Routes.RETURNING_USER, 1);
    }
  }

  private onNewUserClick_() {
    this.finalized_ = true;
    this.landingViewProxy_.recordNewUser();
    navigateTo(Routes.NEW_USER, 1);
  }
}
customElements.define(LandingViewElement.is, LandingViewElement as any);
