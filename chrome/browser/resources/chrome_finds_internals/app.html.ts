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
  <p>Use this page to test the AI notification generation process.</p>
  <ul>
    <li>Enter a custom prompt in the text area below.</li>
    <li>Use <code>{USER_HISTORY}</code> as a placeholder to inject visited
      URLs from the user's history.</li>
    <li>Specify the number of history entries to use. Set to
      <strong>0</strong> for no history. Max is 500.</li>
    <li>Click <strong>Start</strong> to execute the model and see the
      output in the log.</li>
  </ul>
</section>

<section id="controls">
  <div class="input-group">
    <label>
      Number of history entries:
      <input type="number" id="history-count" .value="${this.historyCount_}"
          @change="${this.onHistoryCountChange_}" min="0">
    </label>
  </div>
  <textarea id="prompt-input" rows="10" .value="${this.prompt_}"
      @input="${this.onPromptInput_}"
      placeholder="Paste prompt here..."></textarea>
  <div class="button-row">
    <cr-button id="start-btn" class="action-button"
        @click="${this.onStartClick_}">Start</cr-button>
    <cr-button id="reset-btn" @click="${this.onResetClick_}">
      Reset to Default Prompt
    </cr-button>
    <cr-button id="dump-history-btn" @click="${this.onDumpHistoryClick_}">
      Dump History to JSON
    </cr-button>
  </div>
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
