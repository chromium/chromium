// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContextualTasksAppElement} from './app.js';

// clang-format off
export function getHtml(this: ContextualTasksAppElement) {
  return html`<!--_html_template_start_-->
  ${this.isShownInTab_ ? '' : html`
      <top-toolbar id="toolbar"
          .title="${this.threadTitle_}"
          .attachedTabs="${this.contextTabs_}"
          .darkMode="${this.darkMode_}"
          .isAiPage="${this.isAiPage_}"
          @new-thread-click="${this.onNewThreadClick_}">
      </top-toolbar>
  `}
  <error-page id="errorPage"></error-page>
  <webview id="threadFrame"></webview>
  <zero-state-overlay
      id="zeroStateOverlay"
      .isFirstLoad="${this.isZeroState_}"
      .isSidePanel="${!this.isShownInTab_}">
  </zero-state-overlay>
  <div class="flex-center">
    <div id="relativeThreadHolder">
      <h1 class="thread-header">
          ${this.friendlyZeroStateTitle}
          ${this.friendlyZeroStateSubtitle.length > 0 ?
              html`<br>
              ${this.friendlyZeroStateSubtitle}` : ''}
      </h1>
    </div>
    <contextual-tasks-composebox id="composebox"
          ?hidden="${!this.showComposebox_}"
          .isZeroState="${this.isZeroState_}"
          .isSidePanel="${!this.isShownInTab_}"
          .isLensOverlayShowing="${this.isLensOverlayShowing_}">
    </contextual-tasks-composebox>
  </div>
  <!--_html_template_end_-->`;
}
// clang-format on

/* TODO(crbug.com/470105276): Put composebox into composebox
 * slot for flexbox center formatting instead of temp formatting.
 */
