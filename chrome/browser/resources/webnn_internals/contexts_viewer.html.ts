// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {WebnnInternalsContextsViewerElement} from './contexts_viewer.js';

export function getHtml(this: WebnnInternalsContextsViewerElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="cr-centered-card-container">
  ${this.contexts_.length > 0 ? html`
    <div class="grid-container">
      ${this.contexts_.map(item => html`
        <div class="grid-item">
          <div class="context-detail">
            Context ID: ${item.contextId}
          </div>
          <div class="context-detail">
            Runtime Backend: ${item.contextBackend}
          </div>
          ${item.executionProviders.length > 0 ? html`
            <div class="ep-title">Execution Providers:</div>
            ${item.executionProviders.map((ep, index) => html`
              <div class="ep ${ep.firstSelected ? 'first-selected' : ''}">
                Execution Provider ${index}
                <div class="ep-detail">Name: ${ep.name}</div>
                <div class="ep-detail">Vendor: ${ep.vendor}</div>
                <div class="ep-detail">Hardware Type: ${ep.hardwareType}</div>
                ${ep.version ? html`
                  <div class="ep-detail">Version: ${ep.version}</div>
                ` : ''}
              </div>
            `)}
          ` : ''}
        </div>
      `)}
    </div>
  ` : html`
    <div class="no-context">No active WebNN contexts.</div>
  `}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
