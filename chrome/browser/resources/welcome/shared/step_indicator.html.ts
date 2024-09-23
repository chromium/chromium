// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {StepIndicatorElement} from './step_indicator.js';

export function getHtml(this: StepIndicatorElement) {
  return html`<!--_html_template_start_-->
${this.dots_.map((_item, index) => html`
<span class="${this.getActiveClass_(index) || nothing}"></span>
`)}
<div class="screen-reader-only">${this.getLabel_()}</div>
<!--_html_template_end_-->`;
}
