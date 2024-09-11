// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ManagedUserProfileNoticeValuePropElement} from './managed_user_profile_notice_value_prop.js';

export function getHtml(this: ManagedUserProfileNoticeValuePropElement) {
  return html`<!--_html_template_start_-->
<main class="tangible-sync-style">
  <img id="product-logo" alt="Chrome logo" role="presentation"
      src="chrome://theme/current-channel-logo@2x">
  <h1 class="title">
    ${this.title}
  </h1>
  <p class="subtitle">
    ${this.subtitle}
  </p>
  <div class="pill">
      <img id="avatar" class="avatar" alt="" src="${this.pictureUrl}">
      <div class="text-container">
        <p class="account-name">${this.accountName}</p>
        <p class="email">${this.email}</p>
      </div>
      <cr-icon class="icon" icon="cr:domain"></cr-icon>
  </div>
</main>
<!--_html_template_end_-->`;
}
