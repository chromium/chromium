// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {FeatureShowcaseThemesAndCustomizationStepElement} from './themes_and_customization_step.js';

export function getHtml(
    this: FeatureShowcaseThemesAndCustomizationStepElement) {
  return html`<!--_html_template_start_-->
<feature-showcase-step>
  <slot name="stepper" slot="stepper"></slot>
  <!-- TODO(crbug.com/506842511): Use more general name instead of illustration. !-->
  <div slot="illustration" id="illustration">
    <div class="picker-container">
      <customize-color-scheme-mode></customize-color-scheme-mode>
      <cr-theme-color-picker columns="4"></cr-theme-color-picker>
    </div>
  </div>
  <span slot="title">$i18n{themesAndCustomizationTitle}</span>
  <span slot="description">$i18n{themesAndCustomizationSubtitle}</span>
<if expr="is_win">
  <cr-button slot="button" id="confirm-button" class="action-button"
      @click="${this.onConfirmClick_}" ?disabled="${this.buttonsDisabled}">
    $i18n{themesAndCustomizationApplyTheme}
  </cr-button>
</if>
  <cr-button slot="button" id="skip-button" @click="${this.onSkipClick_}"
      ?disabled="${this.buttonsDisabled}">
    $i18n{themesAndCustomizationNoThanks}
  </cr-button>
<if expr="not is_win">
  <cr-button slot="button" id="confirm-button" class="action-button"
      @click="${this.onConfirmClick_}" ?disabled="${this.buttonsDisabled}">
    $i18n{themesAndCustomizationApplyTheme}
  </cr-button>
</if>
</feature-showcase-step>
<!--_html_template_end_-->`;
}
