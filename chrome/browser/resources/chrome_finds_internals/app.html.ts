// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ChromeFindsInternalsAppElement} from './app.js';

export function getHtml(this: ChromeFindsInternalsAppElement) {
  return html`
<h1>Chrome Finds Internals</h1>

<section id="instructions">
  <h2>Instructions</h2>
  <p>Use this page to inspect and trigger different aspects of the Finds Service:</p>
  <ul>
    <li><strong>Trigger Finds Service</strong>: Executes the full Finds service workflow.</li>
    <li><strong>Trigger Finds Test Notification</strong>: Directly schedules a mock Finds notification for testing.</li>
    <li><strong>Dump History to JSON</strong>: Queries and displays the raw history entries that would be provided to the model.</li>
  </ul>
</section>

<section id="controls">
  <div class="input-group">
    <label>
      Number of history entries:
      <input type="number" id="history-count"
          .value="${this.historyCount_.toString()}"
          @change="${this.onHistoryCountChange_}" min="0">
    </label>
  </div>
    <cr-button id="run-finds-model-btn" class="action-button"
        @click="${this.onRunFindsModelClick_}">
      Trigger Finds Service
    </cr-button>
    <cr-button id="finds-test-notification-btn" class="action-button"
        @click="${this.onTriggerFindsTestNotificationClick_}">
      Trigger Finds Test Notification
    </cr-button>
    <cr-button id="dump-history-btn" @click="${this.onDumpHistoryClick_}">
      Dump History to JSON
    </cr-button>
</section>

<section id="history-dump-section" ?hidden="${!this.historyJson_}">
  <h2>History Dump</h2>
  <div id="history-dump-container">
    <textarea id="history-json-output" rows="10"
        readonly .value="${this.historyJson_}">
    </textarea>
    <div class="button-row">
      <cr-button id="copy-history-btn" @click="${this.onCopyHistoryClick_}">
        Copy to Clipboard
      </cr-button>
    </div>
  </div>
</section>

<section id="log-section">
  <h2>Log</h2>
  <div id="log-container">
    ${this.logs_.map(log => html`
      <div class="log-entry">${log}</div>
    `)}
  </div>
</section>
`;
}
