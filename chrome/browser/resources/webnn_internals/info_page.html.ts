// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {WebnnInternalsInfoPageElement} from './info_page.js';

export function getHtml(this: WebnnInternalsInfoPageElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="cr-centered-card-container">
  <div class="card">
<if expr="is_win">
    <div class="category-title">ONNX Environment Status</div>
    <div class="item onnx-runtime-section">
      ${this.availableExecutionProviders_.length > 0 ? html`
        <div class="environment-created">Environment created</div>
      ` : html`
        <div class="environment-not-created">Environment not created</div>
      `}
      <cr-button @click="${this.onForceOrtEnvCreationClick_}"
          ?disabled="${this.availableExecutionProviders_.length > 0}">
        Force Creation
      </cr-button>
    </div>
</if>
    <div class="category-title">Available Execution Providers</div>
    ${this.availableExecutionProviders_.length > 0 ? html`
      <div class="grid-container-available-eps">
        ${this.availableExecutionProviders_.map(ep => html`
          <div class="item">
            <div class="description">Name:</div>
            <div>${ep.name}</div>
            <div class="description">EP Vendor:</div>
            <div>${ep.vendor}</div>
            <div class="description">Hardware Vendor ID:</div>
            <div>${ep.vendorId}</div>
            <div class="description">Hardware Device ID:</div>
            <div>${ep.deviceId}</div>
            <div class="description">Hardware Type:</div>
            <div>${ep.hardwareType}</div>
            ${ep.version ? html`
              <div class="description">Version:</div>
              <div>${ep.version}</div>
            ` : ''}
          </div>
        `)}
      </div>
    ` : html`
      <div class="noep item">
        No execution providers available or this platform doesn't support
        execution providers.
      </div>
    `}
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
