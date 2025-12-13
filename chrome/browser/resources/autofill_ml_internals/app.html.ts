// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AppElement} from './app.js';

export function getHtml(this: AppElement) {
  // clang-format off
  return html`
    ${this.logEntries_.length > 0 ? html`
      <log-list .logEntries="${this.logEntries_}"
          .selectedLogEntry="${this.selectedLog_}"
          @log-selected="${this.onLogSelected_}">
      </log-list>
      <log-details .log="${this.selectedLog_}"></log-details>
    ` : html`
      <div class="empty-message">
        Trigger some ML predictions while this page is open to record logs.
      </div>
    `}`;
  // clang-format on
}
