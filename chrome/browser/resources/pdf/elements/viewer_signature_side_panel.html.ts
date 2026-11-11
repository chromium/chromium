// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ViewerSignatureSidePanelElement} from './viewer_signature_side_panel.js';

export function getHtml(this: ViewerSignatureSidePanelElement) {
  // TODO(crbug.com/573256245): Replace placeholder string.
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="side-panel-content">
  <h2>Signatures</h2>
  <signature-selector></signature-selector>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
