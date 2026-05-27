// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {VisualGuidedSetterAppElement} from './app.js';

export function getHtml(this: VisualGuidedSetterAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="container">
  <h1>Visual Guided Setter</h1>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
