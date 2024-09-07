// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {ManagedUserProfileNoticeDisclosureElement} from './managed_user_profile_notice_disclosure.js';

export function getHtml(this: ManagedUserProfileNoticeDisclosureElement) {
  return html`<!--_html_template_start_-->
<main class="tangible-sync-style">
  <div id="header-container">
    <div id="avatar-container">
      <img id="avatar" alt="" src="${this.pictureUrl}">
      <div class="work-badge" ?hidden="${!this.showEnterpriseBadge}">
        <cr-icon class="icon" icon="cr:domain"></cr-icon>
      </div>
    </div>
  </div>
  <div id="text-container">
    <h1 class="title">$i18n{profileDisclosureTitle}</h1>
    <p class="subtitle">$i18n{profileDisclosureSubtitle}</p>
  </div>
  <div class="disclaimer-container">
    <section class="disclaimer">
      <cr-icon class="icon" icon="signin:person-outline"></cr-icon>
      <div>
        <h2>$i18n{profileInformationTitle}</h2>
        <p>$i18n{profileInformationDetails}</p>
      </div>
    </section>
    <section class="disclaimer">
      <cr-icon class="icon" icon="cr:phonelink"></cr-icon>
      <div>
        <h2>$i18n{deviceInformationTitle}</h2>
        <p>$i18n{deviceInformationDetails}</p>
      </div>
    </section>
  </div>
</main>
<!--_html_template_end_-->`;
}