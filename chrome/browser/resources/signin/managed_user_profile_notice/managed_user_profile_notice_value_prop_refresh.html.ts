// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ManagedUserProfileNoticeValuePropRefreshElement} from './managed_user_profile_notice_value_prop_refresh.js';

export function getHtml(this: ManagedUserProfileNoticeValuePropRefreshElement) {
  return html`<!--_html_template_start_-->
<main>
  <img id="product-logo" src="images/product-logo.svg"
      alt="$i18n{productLogoAltText}">
  <div class="text-container">
    <h1 id="title" class="title" tabindex="-1">${this.title}</h1>
    <p class="subtitle">${this.subtitle}</p>
  </div>
  <div class="card account-card">
    <img id="avatar" class="avatar" alt="" src="${this.pictureUrl}">
    <div class="account-info">
      <p class="account-name">${this.accountName}</p>
      <p class="email">${this.email}</p>
    </div>
    ${this.showEnterpriseBadge ? html`
      <cr-icon class="icon" icon="cr:domain"
          aria-label="$i18n{enterpriseIconAccessibilityLabel}">
      </cr-icon>
    ` : ''}
  </div>
</main>
<!--_html_template_end_-->`;
}
