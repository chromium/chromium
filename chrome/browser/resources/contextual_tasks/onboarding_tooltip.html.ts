// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContextualTasksOnboardingTooltipElement} from './onboarding_tooltip.js';

export function getHtml(this: ContextualTasksOnboardingTooltipElement) {
  return html`<!--_html_template_start_-->
    <cr-tooltip id="tooltip" role="dialog"
      position="top"
      offset="0"
      fit-to-visible-bounds
      manual-mode>
      <div id="tooltipContent">
        <div class="tooltip-header">
          <div class="tooltip-title">$i18n{onboardingTitle}</div>
        </div>
        <div>$i18n{onboardingBody}
          <a href="$i18n{onboardingLinkUrl}"
            @click="${this.onHelpLinkClick_}">
            $i18n{onboardingLink}
          </a>
        </div>
      </div>
      <div id="buttons">
        <cr-button class="action-button" @click="${this.onTooltipClose_}">
          $i18n{onboardingAcceptButton}
        </cr-button>
      </div>
    </cr-tooltip>
  <!--_html_template_end_-->`;
}
