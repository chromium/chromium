// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AnnotationEntry, ContentAnnotatorInternalsAppElement} from './app.js';

export function getHtml(this: ContentAnnotatorInternalsAppElement) {
  return html`
    <h1>Content Annotator Internals</h1>
    ${this.errorMessage_ ? html`
      <p class="error">${this.errorMessage_}</p>
    ` : this.logContent_.length === 0 ? html`
      <pre>The content annotations cache is currently empty.</pre>
    ` : html`
      <h2>Cached Annotations</h2>
      <table>
        <thead>
          <tr>
            <th>URL</th>
            <th>Title</th>
            <th>Annotations</th>
          </tr>
        </thead>
        <tbody>
          ${this.logContent_.map((entry: AnnotationEntry) => html`
            <tr>
              <td>${entry.url}</td>
              <td>${entry.title}</td>
              <td>
                <pre>${this.formatAnnotations_(entry.annotations)}</pre>
              </td>
            </tr>
          `)}
        </tbody>
      </table>
    `}
  `;
}
