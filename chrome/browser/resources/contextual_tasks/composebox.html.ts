// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContextualTasksComposeboxElement} from './composebox.js';

// clang-format off
export function getHtml(this: ContextualTasksComposeboxElement) {
  return html`<!--_html_template_start_-->
  <div id="composeboxContainer"
    style="
      --composebox-height: ${this.composeboxHeight_}px;
      --composebox-dropdown-height: ${this.composeboxDropdownHeight_}px;"
      >
    ${this.showOnboardingTooltip_ ? html`
      <contextual-tasks-onboarding-tooltip id="onboardingTooltip"
          @onboarding-tooltip-dismissed="${this.onTooltipDismissed_}">
      </contextual-tasks-onboarding-tooltip>` : ''}
    <cr-composebox
      id="composebox"
      ?autofocus="${false}"
      carousel-on-top_
      lens-button-disabled_$="${false}"
      entrypoint-name="ContextualTasks"
      searchbox-layout-mode="TallBottomContext"
      .tabSuggestions="${this.tabSuggestions_}"
    >
    </cr-composebox>
  </div>
  <!--_html_template_end_-->`;
}
// clang-format on
