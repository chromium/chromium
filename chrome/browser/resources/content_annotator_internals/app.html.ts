// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { html } from '//resources/lit/v3_0/lit.rollup.js';

import type { AnnotationEntry, ContentAnnotatorInternalsAppElement } from './app.js';

export function getHtml(this: ContentAnnotatorInternalsAppElement) {
  // TODO(crbug.com/499833072): Add browser tests to verify the UI.
  return html`
    <h1>Content Annotator Internals</h1>
    ${this.errorMessage_ ? html`
      <p class="error">${this.errorMessage_}</p>
    ` : ''}
    ${this.logContent_.length === 0 ? html`
      <pre>The content annotations cache is currently empty.</pre>
    ` :
        html`
      <div class="header-container">
        <h2>Cached Annotations</h2>
        <button id="clear-cache" @click="${this.onClearCacheClick_}">
          Clear Cache
        </button>
      </div>
      <table>
        <thead>
          <tr>
            <th>Visit ID</th>
            <th>Nav Time</th>
            <th>URL</th>
            <th>Title</th>
            <th>Tab ID</th>
            <th>Classifier Results</th>
            <th>Content Annotation</th>
          </tr>
        </thead>
        <tbody>
          ${this.logContent_.map((entry: AnnotationEntry) => html`
            <tr>
              <td>${entry.visit_id}</td>
              <td>${entry.navigation_timestamp}</td>
              <td>${entry.url}</td>
              <td>${entry.title}</td>
              <td>${entry.tab_id || 'N/A'}</td>
              <td>
                <pre>${this.formatJson_(entry.classifier_results)}</pre>
              </td>
              <td>
                <details><pre>${this.formatJson_(entry.content_annotation)}</pre></details>
              </td>
            </tr>
          `)}
        </tbody>
      </table>
    `}
  `;
}
