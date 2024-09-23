// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './shared/action_link_style.css.js';
import './shared/onboarding_background.js';
import './shared/splash_pages_shared.css.js';
import '../strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './landing_view.css.js';
import {getHtml} from './landing_view.html.js';
import type {LandingViewProxy} from './landing_view_proxy.js';
import {LandingViewProxyImpl} from './landing_view_proxy.js';
import {NavigationMixin} from './navigation_mixin.js';
import {navigateTo, Routes} from './router.js';
import type {OnboardingBackgroundElement} from './shared/onboarding_background.js';
import {WelcomeBrowserProxyImpl} from './welcome_browser_proxy.js';

export interface LandingViewElement {
  $: {
    background: OnboardingBackgroundElement,
  };
}

const LandingViewElementBase = NavigationMixin(CrLitElement);

/** @polymer */
export class LandingViewElement extends LandingViewElementBase {
  static get is() {
    return 'landing-view';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      signinAllowed_: {type: Boolean},
    };
  }

  private landingViewProxy_: LandingViewProxy;
  private finalized_: boolean = false;
  protected signinAllowed_: boolean;

  constructor() {
    super();
    this.landingViewProxy_ = LandingViewProxyImpl.getInstance();
    this.signinAllowed_ = loadTimeData.getBoolean('signinAllowed');
  }

  override onRouteEnter() {
    this.finalized_ = false;
    this.landingViewProxy_.recordPageShown();
    this.$.background.play();
  }

  override onRouteExit() {
    this.$.background.pause();
  }

  override onRouteUnload() {
    // Clicking on 'Returning user' will change the URL.
    if (this.finalized_) {
      return;
    }
    this.finalized_ = true;
    this.landingViewProxy_.recordNavigatedAway();
  }

  protected onExistingUserClick_() {
    this.finalized_ = true;
    this.landingViewProxy_.recordExistingUser();
    if (this.signinAllowed_) {
      WelcomeBrowserProxyImpl.getInstance().handleActivateSignIn(
          'chrome://welcome/returning-user');
    } else {
      navigateTo(Routes.RETURNING_USER, 1);
    }
  }

  protected onNewUserClick_() {
    this.finalized_ = true;
    this.landingViewProxy_.recordNewUser();
    navigateTo(Routes.NEW_USER, 1);
  }
}

customElements.define(LandingViewElement.is, LandingViewElement);
