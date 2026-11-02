// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {State} from './managed_user_profile_notice_browser_proxy.js';
import type {ManagedUserProfileNoticeStateRefreshElement} from './managed_user_profile_notice_state_refresh.js';

export function getHtml(this: ManagedUserProfileNoticeStateRefreshElement) {
  return html`<!--_html_template_start_-->
<main>
  <div class="icon-container">
    ${this.isState_(State.PROCESSING) ? html`
      <div class="spinner"></div>
    ` : ''}
    ${this.isState_(State.SUCCESS) ? html`
      <img class="success-icon" alt="">
    ` : ''}
    ${this.isState_(State.TIMEOUT) ? html`
      <img class="timeout-icon" alt="">
    ` : ''}
    ${this.isState_(State.ERROR) ? html`
      <img class="error-icon" alt="">
    ` : ''}
  </div>
  <div class="text-container">
    <h1 id="title" class="title" ?hidden="${!this.getTitle_()}">
      ${this.getTitle_()}
    </h1>
    <p id="subtitle" class="subtitle" ?hidden="${!this.getSubtitle_()}">
      ${this.getSubtitle_()}
    </p>
  </div>
</main>
<!--_html_template_end_-->`;
}
