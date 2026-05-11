// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {FeatureShowcaseDefaultBrowserStepElement} from './default_browser_step.js';

export function getHtml(this: FeatureShowcaseDefaultBrowserStepElement) {
  // clang-format off
  return html`<!--_html_template_start_-->

<feature-showcase-step>
  <img slot="illustration" id="illustration" alt="">
  <span slot="title">$i18n{refreshDefaultBrowserTitle}</span>
  <span slot="description">$i18n{refreshDefaultBrowserSubtitle}</span>
  <cr-button slot="button" id="confirm-button" class="action-button"
      @click="${this.onConfirmButtonClick_}">
    $i18n{refreshDefaultBrowserSetAsDefault}
  </cr-button>
  <cr-button slot="button" id="skip-button" @click="${this.onSkipButtonClick_}">
    $i18n{refreshDefaultBrowserNoThanks}
  </cr-button>
</feature-showcase-step>
<!--_html_template_end_-->`;
  // clang-format on
}
