// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {FeatureShowcaseStepElement} from './feature_showcase_step.js';

export function getHtml(this: FeatureShowcaseStepElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="content-child" id="showcase-illustration">
  <slot name="illustration"></slot>
</div>

<div class="content-child" id="showcase-description">
<!-- TODO(crbug.com/500272103): Substitute placeholder with real stepper. -->
  <div id="stepper-placeholder">
    <slot name="header-icon"></slot>
  </div>
  <div id="showcase-text">
    <h1 class="title"><slot name="title"></slot></h1>
    <p class="subtitle"><slot name="description"></slot></p>
    <div id="button-container">
      <slot name="button"></slot>
    </div>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
