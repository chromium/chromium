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
            <span class="label">Context ID:</span> ${item.contextId}
          </div>
          <div class="context-detail">
            <span class="label">Runtime Backend:</span> ${item.contextBackend}
          </div>
          <div class="context-detail">
            <span class="label">Requested Device:</span>
            ${item.requestedDeviceType}
          </div>
          ${item.executionProviders.length > 0 ? html`
            <div class="ep-title">Execution Providers:</div>
            ${item.executionProviders.map((ep, index) => html`
              <div class="ep ${ep.firstSelected ? 'first-selected' : ''}">
                <div class="ep-heading">Execution Provider ${index}</div>
                <div class="ep-detail">
                  <span class="label">Name:</span> ${ep.name}
                </div>
                <div class="ep-detail">
                  <span class="label">EP Vendor:</span> ${ep.vendor}
                </div>
                <div class="ep-detail">
                  <span class="label">Hardware Vendor ID:</span> ${ep.vendorId}
                </div>
                <div class="ep-detail">
                  <span class="label">Hardware Device ID:</span> ${ep.deviceId}
                </div>
                <div class="ep-detail">
                  <span class="label">Hardware Type:</span> ${ep.hardwareType}
                </div>
                ${ep.version ? html`
                  <div class="ep-detail">
                    <span class="label">EP Version:</span> ${ep.version}
                  </div>
                ` : ''}
                ${ep.driverVersion ? html`
                  <div class="ep-detail">
                    <span class="label">Driver Version:</span>
                    ${ep.driverVersion}
                  </div>
                ` : ''}
                ${ep.libraryPath ? html`
                  <div class="ep-detail library-path">
                    <span class="label">Library Path:</span> ${ep.libraryPath}
                  </div>
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
