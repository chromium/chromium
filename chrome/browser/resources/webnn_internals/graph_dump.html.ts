// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {WebnnInternalsGraphDumpElement} from './graph_dump.js';

export function getHtml(this: WebnnInternalsGraphDumpElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="cr-centered-card-container">
  <div class="card">
    <div>Export ML Graphs for Debugging (supported on debug builds only):</div>
    <cr-toggle class="control"
        ?checked="${this.recordGraphEnabled_}"
        @change="${this.onToggleValueChange_}">
    </cr-toggle>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
