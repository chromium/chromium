// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {FeatureShowcasePasswordManagerStepElement} from './password_manager_step.js';

export function getHtml(this: FeatureShowcasePasswordManagerStepElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<feature-showcase-step>
  <slot name="stepper" slot="stepper"></slot>
  <img slot="illustration" id="illustration"
      alt="$i18n{passwordManagerIllustrationA11yLabel}">
  <span slot="title">$i18n{passwordManagerTitle}</span>
  <span slot="description">$i18n{passwordManagerSubtitle}</span>
  <cr-button slot="button" id="confirm-button" class="action-button"
      @click="${this.onConfirmClick_}"
      ?disabled="${this.buttonsDisabled}">
    $i18n{passwordManagerAddToToolbar}
  </cr-button>
  <cr-button slot="button" id="skip-button" @click="${this.onSkipClick_}"
      ?disabled="${this.buttonsDisabled}">
    $i18n{passwordManagerNoThanks}
  </cr-button>
</feature-showcase-step>
<!--_html_template_end_-->`;
  // clang-format on
}
