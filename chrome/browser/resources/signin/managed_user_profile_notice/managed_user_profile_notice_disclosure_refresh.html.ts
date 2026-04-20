// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {ManagedUserProfileNoticeDisclosureRefreshElement} from './managed_user_profile_notice_disclosure_refresh.js';

export function getHtml(this: ManagedUserProfileNoticeDisclosureRefreshElement) {
  return html`<!--_html_template_start_-->
<main>
  <div id="headerContainer">
    ${this.shouldShowAnimations_() ? html`
      <cr-lottie id="avatarAnimation"
          animation-url="${this.getAnimationUrl_()}"
          single-loop
          ?autoplay="${!this.shouldDisableAnimations_()}">
      </cr-lottie>
    ` : ''}
    <div id="avatarContainer">
      <img id="avatar" alt="$i18n{avatarAccessibilityLabel}"
          src="${this.pictureUrl}">
      ${this.showEnterpriseBadge ? html`
        <div class="work-badge">
          <cr-icon class="icon" icon="cr:domain"
              aria-label="$i18n{enterpriseIconAccessibilityLabel}">
          </cr-icon>
        </div>
      ` : ''}
    </div>
  </div>
  <div id="textContainer">
    <h1 id="title" class="title" tabindex="-1">${this.title}</h1>
    <p id="subtitle" class="subtitle">${this.subtitle}</p>
  </div>
  <div class="disclaimer-container">
    <section class="disclaimer">
      <cr-icon class="icon" icon="signin:person-outline"></cr-icon>
      <h2>$i18n{profileInformationTitle}</h2>
      <p>$i18n{profileInformationDetails}</p>
    </section>
    <section class="disclaimer">
      <cr-icon class="icon" icon="cr:phonelink"></cr-icon>
      <h2>$i18n{deviceInformationTitle}</h2>
      <p>$i18n{deviceInformationDetails}</p>
    </section>
  </div>
</main>
<!--_html_template_end_-->`;
}
