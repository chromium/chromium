// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {StepIndicatorElement} from './step_indicator.js';

export function getHtml(this: StepIndicatorElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.dots_.map((_item, index) => html`
  <span class="${this.getActiveClass_(index)}"></span>
`)}
<div class="screen-reader-only">
  ${this.computeA11yLabel_()}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
