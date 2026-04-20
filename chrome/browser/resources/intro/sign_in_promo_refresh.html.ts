// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SignInPromoRefreshElement} from './sign_in_promo_refresh.js';

export function getHtml(this: SignInPromoRefreshElement) {
  // clang-format off
  // TODO(crbug.com/504301457): Enable animations once there is a mechanism to
  // pause/stop them.
  return html`<!--_html_template_start_-->
<cr-lottie id="left-animation" class="animation"
    animation-url="${this.getAnimationUrl_('left')}"
    ?autoplay=false>
</cr-lottie>
<cr-lottie id="right-animation" class="animation"
    animation-url="${this.getAnimationUrl_('right')}"
    ?autoplay=false>
</cr-lottie>
<cr-lottie id="bottom-animation" class="animation"
    animation-url="${this.getAnimationUrl_('bottom')}"
    ?autoplay=false>
</cr-lottie>

${this.isTopRightCornerVariation_() ? html`
  <div id="top-right-corner-container">
    <cr-button id="declineSignInButton"
        class="tangible-button ${this.usePrimaryAndTonalButtonsForPromos_ ?
          'tonal-button' : ''}"
        ?disabled="${this.shouldDisableButtons_()}"
        @click="${this.onDeclineSignInButtonClick_}">
      $i18n{declineSignInButtonTitle}
    </cr-button>
  </div>
` : ''}

<div id="product-logo-container">
  <img id="product-logo-animation" src="images/product-logo-animation.svg"
    alt="$i18n{productLogoAltText}">
</div>

<h1 class="title fade-in">$i18n{pageTitle}</h1>

<div id="textContainer">
  <div id="benefitCardsContainer">
    ${this.benefitCards_.map(item => html`
      <div class="benefit-card fade-in">
        <div id="${item.iconId}" class="cr-icon"></div>
        <h2>${item.title}</h2>
        <p class="benefit-card-description">${item.description}</p>
      </div>
    `)}
  </div>

  <div id="managedDeviceDisclaimer"
      ?hidden="${!this.isDeviceManaged_}"
      class="${this.getDisclaimerVisibilityClass_()}">
    <div id="iconContainer">
      <cr-icon icon="cr:domain"></cr-icon>
    </div>
    <p id="disclaimerText">${this.managedDeviceDisclaimer_}</p>
  </div>
</div>

<div id="buttonRow" class="fade-in">
  ${this.isTopRightCornerVariation_() ? html`
    <p id="create-account-disclaimer">$i18n{createAccountDisclaimer}</p>
  ` : ''}
  <div id="buttonContainer">
    ${this.isDefaultVariation_() ? html`
    <cr-button id="declineSignInButton"
        class="tangible-button ${this.usePrimaryAndTonalButtonsForPromos_ ?
            'tonal-button' : ''}"
        ?disabled="${this.shouldDisableButtons_()}"
        @click="${this.onDeclineSignInButtonClick_}">
      $i18n{declineSignInButtonTitle}
    </cr-button>
    ` : ''}
    <cr-button id="acceptSignInButton" class="tangible-button action-button"
        ?disabled="${this.shouldDisableButtons_()}"
        @click="${this.onAcceptSignInButtonClick_}">
      $i18n{acceptSignInButtonTitle}
    </cr-button>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
