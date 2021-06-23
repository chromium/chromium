// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared_css.js';
import './base_page.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'onboarding-landing-page' is the main landing page for the shimless rma
 * process.
 */
export class OnboardingLandingPage extends PolymerElement {
  static get is() {
    return 'onboarding-landing-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  /** @override */
  ready() {
    super.ready();
  }
};

customElements.define(OnboardingLandingPage.is, OnboardingLandingPage);
