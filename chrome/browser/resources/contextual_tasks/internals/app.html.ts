// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContextualTasksInternalsAppElement} from './app.js';

export function getHtml(this: ContextualTasksInternalsAppElement) {
  // clang-format off
  return html`
<h1>Contextual Tasks Internals</h1>
<div class="model-options">
  <select id="tabSelectionModeSelect" class="md-select"
      value="${this.tabSelectionMode_}" @change="${this.onTabSelectionModeChanged_}">
    <option value="kEmbeddingsMatch">Embeddings Match</option>
    <option value="kMultiSignalScoring">Multi Signal Scoring</option>
  </select>
</div>
<cr-textarea type="text" id="textInput" label="Query"
    placeholder="Type query here..."
    .value="${this.query_}" @value-changed="${this.onQueryChanged_}">
</cr-textarea>
<div>
  <cr-button class="action-button" ?disabled="${this.isQueryPending_}"
      @click="${this.onSubmitClick_}">
    Submit
  </cr-button>
</div>
<div>
  <div>Relevant Tabs:</div>
  <ul>
  ${this.relevantTabs_.map(item => html`
    <li>
      <a href="${item.url}" target="_blank">${item.title}</a>
    </li>
  `)}
  </ul>
</div>
<div>
  <div>Log Messages:</div>
  <table>
    <thead>
      <tr>
        <th class="time">Time</th>
        <th class="source-location">Source Location</th>
        <th class="message">Log Message</th>
      </tr>
    </thead>
    <tbody>
      ${this.eventLogMessages_.map(item => html`
        <tr>
          <td class="time">${item.eventTime.toLocaleTimeString()}</td>
          <td class="source-location">
            <a href="${item.sourceLinkURL}">${item.sourceLinkText}</a>
          </td>
          <td class="message">${item.message}</td>
        </tr>`)}
    </tbody>
  </table>
</div>
`;
  // clang-format on
}
