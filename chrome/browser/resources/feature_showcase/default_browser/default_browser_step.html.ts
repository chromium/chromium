// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {FeatureShowcaseDefaultBrowserStepElement} from './default_browser_step.js';

export function getHtml(this: FeatureShowcaseDefaultBrowserStepElement) {
  return html`<!--_html_template_start_-->

<feature-showcase-step>
  <slot name="stepper" slot="stepper"></slot>
  <img slot="illustration" id="illustration" alt="">
  <span slot="title">$i18n{defaultBrowserTitle}</span>
  <span slot="description">$i18n{defaultBrowserSubtitle}</span>
<if expr="is_win">
  <cr-button slot="button" id="confirm-button" class="action-button"
      @click="${this.onConfirmClick_}" ?disabled="${this.buttonsDisabled}">
    $i18n{defaultBrowserSetAsDefault}
  </cr-button>
</if>
  <cr-button slot="button" id="skip-button" @click="${this.onSkipClick_}"
      ?disabled="${this.buttonsDisabled}">
    $i18n{defaultBrowserNoThanks}
  </cr-button>
<if expr="not is_win">
  <cr-button slot="button" id="confirm-button" class="action-button"
      @click="${this.onConfirmClick_}" ?disabled="${this.buttonsDisabled}">
    $i18n{defaultBrowserSetAsDefault}
  </cr-button>
</if>
</feature-showcase-step>
<!--_html_template_end_-->`;
}
