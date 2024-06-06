// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SigninViewElement} from './signin_view.js';

export function getHtml(this: SigninViewElement) {
  return html`<!--_html_template_start_-->
<div id="container">
  <onboarding-background id="background" class="fade-in">
  </onboarding-background>
  <div id="text">
    <div class="header">
      <h1 tabindex="-1">$i18n{signInHeader}</h1>
      <div class="subheading">$i18n{signInSubHeader}</div>
    </div>
    <cr-button class="action-button" @click="${this.onSignInClick_}">
      $i18n{signIn}
    </cr-button>
    <button class="action-link" @click="${this.onNoThanksClick_}">
      $i18n{noThanks}
    </button>
  </div>
</div>
<!--_html_template_end_-->`;
}
