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
      <top-toolbar .title="${this.threadTitle_}"
          .attachedTabs="${this.contextTabs_}"
          @signin-click="${this.removeGsc_}"
          @new-thread-click="${this.onNewThreadClick_}"
          @thread-history-click="${this.onThreadHistoryClick_}">
      </top-toolbar>
  `}
  <webview id="threadFrame" src="${this.threadUrl_}"></webview>
  <contextual-tasks-composebox id="composebox"></contextual-tasks-composebox>
  <!--_html_template_end_-->`;
}
// clang-format on
