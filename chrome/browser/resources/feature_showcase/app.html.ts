// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {FeatureShowcaseAppElement} from './app.js';

export function getHtml(this: FeatureShowcaseAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-view-manager id="viewManager">
  ${this.hasStep_('example') ? html`
      <feature-showcase-example-step id="example" slot="view"
          @step-completed="${this.onStepCompleted_}">
      </feature-showcase-example-step>
  ` : ''}
</cr-view-manager>
<!--_html_template_end_-->`;
  // clang-format on
}
