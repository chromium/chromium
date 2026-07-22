// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {FeatureShowcaseAppElement} from './app.js';

export function getHtml(this: FeatureShowcaseAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-lottie class="animation" id="rightAnimation"
    animation-url="${this.getAnimationUrl_('right')}"
    single-loop>
</cr-lottie>
<cr-lottie class="animation" id="bottomAnimation"
    animation-url="${this.getAnimationUrl_('bottom')}"
    single-loop>
</cr-lottie>

<cr-view-manager id="viewManager">
  ${this.hasStep_('default-browser') ? html`
      <feature-showcase-default-browser-step id="default-browser" slot="view"
          @step-completed="${this.onStepCompleted_}"
          ?buttons-disabled="${this.areButtonsDisabled_}">
        <feature-showcase-stepper slot="stepper"
            .steps="${this.steps}"
            .activeIndex="${this.activeStepIndex}">
        </feature-showcase-stepper>
      </feature-showcase-default-browser-step>
  ` : ''}

  ${this.hasStep_('gemini') ? html`
      <feature-showcase-gemini-step id="gemini" slot="view"
          @step-completed="${this.onStepCompleted_}"
          ?buttons-disabled="${this.areButtonsDisabled_}">
        <feature-showcase-stepper slot="stepper"
            .steps="${this.steps}"
            .activeIndex="${this.activeStepIndex}">
        </feature-showcase-stepper>
      </feature-showcase-gemini-step>
  ` : ''}

  ${this.hasStep_('password-manager') ? html`
      <feature-showcase-password-manager-step id="password-manager" slot="view"
          @step-completed="${this.onStepCompleted_}"
          ?buttons-disabled="${this.areButtonsDisabled_}">
        <feature-showcase-stepper slot="stepper"
            .steps="${this.steps}"
            .activeIndex="${this.activeStepIndex}">
        </feature-showcase-stepper>
      </feature-showcase-password-manager-step>
  ` : ''}

  ${this.hasStep_('google-lens') ? html`
      <feature-showcase-google-lens-step id="google-lens" slot="view"
          @step-completed="${this.onStepCompleted_}"
          ?buttons-disabled="${this.areButtonsDisabled_}">
        <feature-showcase-stepper slot="stepper"
            .steps="${this.steps}"
            .activeIndex="${this.activeStepIndex}">
        </feature-showcase-stepper>
      </feature-showcase-google-lens-step>
  ` : ''}

  ${this.hasStep_('themes-and-customization') ? html`
      <feature-showcase-themes-and-customization-step
          id="themes-and-customization"
          slot="view"
          @step-completed="${this.onStepCompleted_}"
          ?buttons-disabled="${this.areButtonsDisabled_}">
        <feature-showcase-stepper slot="stepper"
            .steps="${this.steps}"
            .activeIndex="${this.activeStepIndex}">
        </feature-showcase-stepper>
      </feature-showcase-themes-and-customization-step>
  ` : ''}
</cr-view-manager>
<!--_html_template_end_-->`;
  // clang-format on
}
