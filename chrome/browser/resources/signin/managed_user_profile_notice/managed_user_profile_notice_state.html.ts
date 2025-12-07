// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ManagedUserProfileNoticeStateElement} from './managed_user_profile_notice_state.js';

export function getHtml(this: ManagedUserProfileNoticeStateElement) {
  return html`<!--_html_template_start_-->
<main class="tangible-sync-style">
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
