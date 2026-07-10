// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {FeatureShowcaseGoogleLensStepElement} from './google_lens_step.js';

export function getHtml(this: FeatureShowcaseGoogleLensStepElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<feature-showcase-step>
  <slot name="stepper" slot="stepper"></slot>
  <img slot="illustration" id="illustration" alt="">
  <span slot="title">$i18n{lensTitle}</span>
  <span slot="description">$i18n{lensSubtitle}</span>
  <span slot="disclosure">$i18n{lensDisclosure}</span>
  <if expr="is_win">
    <cr-button slot="button" id="confirm-button" class="action-button"
        @click="${this.onConfirmClick_}"
        ?disabled="${this.buttonsDisabled}">
      $i18n{lensYesImIn}
    </cr-button>
  </if>
    <cr-button slot="button" id="skip-button"
        @click="${this.onSkipClick_}"
        ?disabled="${this.buttonsDisabled}">
      $i18n{lensNotNow}
    </cr-button>
  <if expr="not is_win">
    <cr-button slot="button" id="confirm-button" class="action-button"
        @click="${this.onConfirmClick_}"
        ?disabled="${this.buttonsDisabled}">
      $i18n{lensYesImIn}
    </cr-button>
  </if>
</feature-showcase-step>
<!--_html_template_end_-->`;
  // clang-format on
}
