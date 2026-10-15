// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SignalsDisclaimerRefreshElement} from './signals_disclaimer_refresh.js';

export function getHtml(this: SignalsDisclaimerRefreshElement) {
  return html`<!--_html_template_start_-->
<div id="avatarContainer" class="avatar-container">
  <img id="avatar" class="avatar" alt="$i18n{avatarAccessibilityLabel}"
      src="${this.pictureUrl}">
  <div class="work-badge">
    <cr-icon icon="cr:domain"
        aria-label="$i18n{enterpriseIconAccessibilityLabel}">
    </cr-icon>
  </div>
</div>
<div id="textContainer" class="text-container">
  <h1 id="title" class="title" tabindex="-1">
    $i18n{signalsDisclaimerTitle}
  </h1>
  <p id="subtitle" class="subtitle">
    $i18n{signalsDisclaimerSubtitle}
    <span id="learnMoreLink" class="link" role="link" tabindex="0"
        @click="${this.onLearnMoreClick_}"
        @keydown="${this.onLearnMoreKeydown_}">
      $i18n{learnMore}
    </span>
  </p>
</div>
<div class="disclaimer-container">
  <section class="disclaimer">
    <cr-icon class="icon" icon="signin:account-circle"></cr-icon>
    <h2>$i18n{profileInformationTitle}</h2>
    <p>$i18n{signalsDisclaimerProfileInformationDetails}</p>
  </section>
  <section class="disclaimer">
    <cr-icon class="icon" icon="cr:computer"></cr-icon>
    <h2>$i18n{deviceInformationTitle}</h2>
    <p>$i18n{signalsDisclaimerDeviceInformationDetails}</p>
  </section>
</div>
<!--_html_template_end_-->`;
}
