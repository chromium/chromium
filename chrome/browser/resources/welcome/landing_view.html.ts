// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {LandingViewElement} from './landing_view.js';

export function getHtml(this: LandingViewElement) {
  return html`<!--_html_template_start_-->
<div id="container">
  <onboarding-background id="background" class="fade-in">
  </onboarding-background>
  <div id="text">
    <div class="header">
      <h1 class="fade-in" tabindex="-1">$i18n{landingTitle}</h1>
      <div class="subheading fade-in">$i18n{landingDescription}</div>
    </div>
    <cr-button class="action-button fade-in" @click="${this.onNewUserClick_}">
      $i18n{landingNewUser}
    </cr-button>
    <button class="action-link fade-in" @click="${this.onExistingUserClick_}">
      <span ?hidden="${!this.signinAllowed_}">$i18n{landingExistingUser}</span>
      <span ?hidden="${this.signinAllowed_}">$i18n{skip}</span>
    </button>
  </div>
</div>
<!--_html_template_end_-->`;
}
