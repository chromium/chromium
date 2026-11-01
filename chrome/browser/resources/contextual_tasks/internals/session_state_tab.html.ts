// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContextualTasksSessionStateTabElement} from './session_state_tab.js';

export function getHtml(this: ContextualTasksSessionStateTabElement) {
  // clang-format off
  return html`
<div class="card">
  <div class="session-state-header">
    <div>
      <h2>Tracked WebContents &amp; Session Handles</h2>
      <p class="section-subtitle">
        Inspects open tabs and cached side panels with an attached
        ContextualTasksWebContentsUserData or ContextualSearchSessionHandle.
      </p>
    </div>
    <cr-button id="refreshSessionStateButton" class="action-button"
        @click="${this.onRefreshSessionStateClick_}">
      Refresh
    </cr-button>
  </div>
</div>
<div class="card empty-state-card">
  <p class="empty-state-text">
    Session and user data inspection will be displayed here.
  </p>
</div>
`;
  // clang-format on
}
