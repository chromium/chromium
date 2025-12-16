// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContextualTasksOnboardingTooltipElement} from './onboarding_tooltip.js';

export function getHtml(this: ContextualTasksOnboardingTooltipElement) {
  return html`<!--_html_template_start_-->
    <cr-tooltip id="tooltip"
      position="left"
      offset="0"
      fit-to-visible-bounds
      manual-mode>
      <div id="tooltipContent">
        <div class="tooltip-header">
          <div class="tooltip-title">${this.onboardingTitle_}</div>
          <cr-icon-button class="icon-clear" iron-icon="cr:close"
              @click="${this.onTooltipClose_}">
          </cr-icon-button>
        </div>
        <div>${this.onboardingBody_}
          <a href="${this.onboardingLinkUrl_}"
              @click="${this.onHelpLinkClick_}">${
                this.onboardingLink_}</a></div>
      </div>
    </cr-tooltip>
  <!--_html_template_end_-->`;
}
