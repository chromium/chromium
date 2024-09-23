// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ProfileTypeChoiceElement} from './profile_type_choice.js';

export function getHtml(this: ProfileTypeChoiceElement) {
  return html`<!--_html_template_start_-->
<div id="headerContainer"
    .style="--theme-frame-color:${this.profileThemeInfo.themeFrameColor};
            --theme-text-color:${this.profileThemeInfo.themeFrameTextColor};">
  <!-- TODO(crbug.com/40267173): remove theme info across the profile picker -->
  <cr-icon-button id="backButton" iron-icon="cr:arrow-back"
      @click="${this.onBackClick_}"
      aria-label="${this.getBackButtonAriaLabel_()}"
      title="${this.getBackButtonAriaLabel_()}"
      ?disabled="${this.profileCreationInProgress}">
  </cr-icon-button>
  <div id="avatarContainer">
    <img class="avatar" alt=""
        src="${this.profileThemeInfo.themeGenericAvatar}">
  </div>
</div>
<div class="title-container">
  <h1 class="title">$i18n{profileTypeChoiceTitle}</h1>
  <div class="subtitle">$i18n{profileTypeChoiceSubtitle}</div>
</div>
<div id="actionContainer">
  <cr-button id="signInButton" class="action-button"
      @click="${this.onSignInClick_}"
      ?disabled="${this.profileCreationInProgress}">
    $i18n{signInButtonLabel}
  </cr-button>
  <cr-button id="notNowButton" @click="${this.onNotNowClick_}"
      ?disabled="${this.profileCreationInProgress}">
    $i18n{notNowButtonLabel}
  </cr-button>
</div>

${this.managedDeviceDisclaimer_ ? html`
  <div id="infoContainer">
    <div class="info-box">
      <div class="icon-container">
        <cr-icon icon="cr:domain"></cr-icon>
      </div>
      <p>$i18n{managedDeviceDisclaimer}</p>
    </div>
  </div>
` : ''}
<!--_html_template_end_-->`;
}
