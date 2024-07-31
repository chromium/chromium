// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {SignInPromoElement} from './sign_in_promo.js';

export function getHtml(this: SignInPromoElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<img class="tangible-sync-style-left-banner" alt="">
<img class="tangible-sync-style-right-banner" alt="">

<div id="safeZone" class="tangible-sync-style">
  <div id="contentArea">
    <img id="product-logo" src="images/product-logo.svg"
        alt="$i18n{productLogoAltText}">
    <h1 class="title fade-in">$i18n{pageTitle}</h1>
    <p class="subtitle fade-in">$i18n{pageSubtitle}</p>
    <div id="benefit-cards-container">
      ${this.benefitCards_.map(item => html`
        <div class="benefit-card fade-in">
          <div id="${item.iconId}" class="cr-icon" title=""></div>
          <h2>${item.title}</h2>
          <p class="benefit-card-description">${item.description}</p>
        </div>
      `)}
    </div>
    <div id="managedDeviceDisclaimer"
        ?hidden="${!this.isDeviceManaged_}"
        class="${this.getDisclaimerVisibilityClass_()}">
      <div id="icon-container">
        <cr-icon icon="cr:domain" alt=""></cr-icon>
      </div>
      <p id="disclaimerText">${this.managedDeviceDisclaimer_}</p>
    </div>
  </div>
</div>

<div id="buttonRow" class="fade-in tangible-sync-style">
  <div id="buttonContainer">
    <cr-button id="declineSignInButton"
        ?disabled="${this.areButtonsDisabled_()}"
        @click="${this.onContinueWithoutAccountClick_}">
      $i18n{declineSignInButtonTitle}
    </cr-button>
    <cr-button id="acceptSignInButton" class="action-button"
        ?disabled="${this.areButtonsDisabled_()}"
        @click="${this.onContinueWithAccountClick_}">
      $i18n{acceptSignInButtonTitle}
    </cr-button>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
