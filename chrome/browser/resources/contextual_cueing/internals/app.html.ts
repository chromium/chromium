// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContextualCueingInternalsAppElement} from './app.js';

export function getHtml(this: ContextualCueingInternalsAppElement) {
  return html`
<h1>Contextual Cueing Internals</h1>
<div class="card">
  <h2>Generated Cues (Reverse Chronological)</h2>
  <div class="cues-container">
    ${this.shownCues_.length === 0 ? html`
      <div class="empty-state">No cues shown yet.</div>
    ` : html`
      ${this.shownCues_.map((item, index) => html`
        <div class="cue-item">
          <div class="cue-details">
            ${item.cuj ? html`
              <div class="cue-field">
                <strong>CUJ:</strong>
                <span>${item.cuj}</span>
              </div>
            ` : ''}
            ${item.anchoredMessageText ? html`
              <div class="cue-field">
                <strong>Message:</strong>
                <span>${item.anchoredMessageText}</span>
              </div>
            ` : ''}
            ${item.actionText ? html`
              <div class="cue-field">
                <strong>Action:</strong>
                <span>${item.actionText}</span>
              </div>
            ` : ''}
            ${item.url ? html`
              <div class="cue-field">
                <strong>URL:</strong>
                <a class="url-text" href="${item.url}" target="_blank">
                  ${item.url}
                </a>
              </div>
            ` : ''}
            ${item.prompt ? html`
              <div class="cue-field">
                <strong>Prompt:</strong>
                <span>${item.prompt}</span>
              </div>
            ` : ''}
          </div>
          <cr-button data-index="${index}" @click="${this.onFeedbackClick_}">
            Give Feedback
          </cr-button>
        </div>
      `)}
    `}
  </div>
</div>`;
}
