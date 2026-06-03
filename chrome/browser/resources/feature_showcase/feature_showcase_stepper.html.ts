// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {FeatureShowcaseStepperElement} from './feature_showcase_stepper.js';

export function getHtml(this: FeatureShowcaseStepperElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="container" role="progressbar"
    aria-valuemin="1"
    aria-valuemax="${this.steps.length}"
    aria-valuenow="${this.activeIndex + 1}">
  ${this.steps.length < 3 ? html`
    <div class="step">
      <img src="/images/product-logo.svg" alt="">
    </div>
  ` : this.steps.map((_item, index) => html`
    <div class="step">
      ${index < this.activeIndex ? html`
        <cr-icon icon="cr:check"></cr-icon>
      ` : index === this.activeIndex ? html`
        <img src="/images/product-logo.svg" alt="">
      ` : html`
        <div class="dot"></div>
      `}
    </div>
  `)}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
