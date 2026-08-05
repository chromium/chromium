// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SignalsDisclaimerElement} from './signals_disclaimer.js';

export function getHtml(this: SignalsDisclaimerElement) {
  // clang-format off
  return html`
<!--_html_template_start_-->
<main class="${this.isModalDialog ? 'modal-dialog' : 'profile-picker'}">
  <div id="header-container">
    <div id="avatar-container" class="tangible-sync-style">
      <img id="avatar" alt="" src="${this.pictureUrl}">
      <div class="work-badge">
        <cr-icon class="icon" icon="cr:domain"></cr-icon>
      </div>
    </div>
  </div>
  <div id="text-container">
    <h1 class="title" tabindex="-1">${this.i18n('signalsDisclaimerTitle')}</h1>
    <p class="subtitle">
      ${this.i18n('signalsDisclaimerSubtitle')}
      <span id="learnMoreLink" class="link" role="link" tabindex="0"
          @click="${this.onLearnMoreClick}"
          @keydown="${this.onLearnMoreKeydown}">
        ${this.i18n('learnMore')}
      </span>
    </p>
  </div>
  <div class="disclaimer-container">
    <section class="disclaimer">
      <cr-icon class="icon" icon="signin:account-circle"></cr-icon>
      <div>
        <h2>${this.i18n('profileInformationTitle')}</h2>
        <p>${this.i18n('signalsDisclaimerProfileInformationDetails')}</p>
      </div>
    </section>
    <section class="disclaimer">
      <cr-icon class="icon" icon="cr:computer"></cr-icon>
      <div>
        <h2>${this.i18n('deviceInformationTitle')}</h2>
        <p>${this.i18n('signalsDisclaimerDeviceInformationDetails')}</p>
      </div>
    </section>
  </div>
</main>
<!--_html_template_end_-->`;
  // clang-format on
}
