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
  <div class="container">
    <span class="mode-label">Tab selection mode: </span>
    <select id="tabSelectionModeSelect" class="md-select"
        value="${this.tabSelectionMode_}" @change="${this.onTabSelectionModeChanged_}">
      <option value="kEmbeddingsMatch">Embeddings Match</option>
      <option value="kMultiSignalScoring">Multi Signal Scoring</option>
    </select>
  </div>
  <div class="container">
    <span class="slider-percentage">Min model score: ${this.minModelScore_}</span>
    <cr-slider id="minModelScoreSlider" aria-label="Min model score"
        min="0" max="1.0"
        .value="${this.minModelScore_}"
        @cr-slider-value-changed="${this.onMinModelScoreChanged_}">
    </cr-slider>
  </div>
</div>
<cr-textarea type="text" id="textInput" label=""
    placeholder="Type query here..."
    .value="${this.query_}" @value-changed="${this.onQueryChanged_}">
</cr-textarea>
<cr-button class="action-button" ?disabled="${this.isQueryPending_}"
    @click="${this.onSubmitClick_}">
    Submit
</cr-button>
<div class="container">
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
