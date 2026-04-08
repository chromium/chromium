// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AccessibilityAnnotatorInfoElement} from './accessibility_annotator_info.js';

export function getHtml(this: AccessibilityAnnotatorInfoElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="container">
  <h1>Accessibility annotator Info</h1>
  <p>This is a test page for the accessibility annotator info dialog.</p>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
