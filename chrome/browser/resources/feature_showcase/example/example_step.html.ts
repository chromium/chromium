// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {FeatureShowcaseExampleStepElement} from './example_step.js';

export function getHtml(this: FeatureShowcaseExampleStepElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<feature-showcase-step>
  <img slot="illustration" id="illustration" alt="">
  <span slot="title">Feature Showcase Example title</span>
  <span slot="description">Feature Showcase Example description</span>
  <cr-button slot="button" id="confirm-button" class="action-button"
      @click="${this.onButtonClick_}">
    Feature Showcase Example button
  </cr-button>
</feature-showcase-step>
<!--_html_template_end_-->`;
  // clang-format on
}
