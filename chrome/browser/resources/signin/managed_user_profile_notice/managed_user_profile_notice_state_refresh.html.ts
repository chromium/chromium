// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ManagedUserProfileNoticeStateRefreshElement} from './managed_user_profile_notice_state_refresh.js';

export function getHtml(this: ManagedUserProfileNoticeStateRefreshElement) {
  return html`<!--_html_template_start_-->
<main>
  <div class="icon-container">
    <slot></slot>
  </div>
  <div class="text-container">
    <h1 class="title" ?hidden="${!this.title}">${this.title}</h1>
    <p class="subtitle" ?hidden="${!this.subtitle}">${this.subtitle}</p>
  </div>
</main>
<!--_html_template_end_-->`;
}
