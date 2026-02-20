// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContextualTasksComposeboxElement} from './composebox.js';

// clang-format off
export function getHtml(this: ContextualTasksComposeboxElement) {
  /* 'suggestions' ternary is to change DOM order:
   *  Side panel has suggestions appear between header and composebox.
   *  Full tab has suggestions appear below the composebox (which is below the header).
   */
  return html`<!--_html_template_start_-->
    ${this.isSidePanel && this.enableNativeZeroStateSuggestions ? html`
      <cr-composebox-dropdown
          id="contextualTasksSuggestionsContainer"
          role="listbox"
          .result="${this.zeroStateSuggestions_}"
          .maxSuggestions="${5}"
          ?hidden="${!this.isZeroState}">
      </cr-composebox-dropdown>
    ` : ''}
    <div id="composeboxContainer"
      style="
        --composebox-height: ${this.composeboxHeight_}px;
        --composebox-dropdown-height: ${this.composeboxDropdownHeight_}px;"
        >
      ${this.showOnboardingTooltip_ ? html`
        <contextual-tasks-onboarding-tooltip id="onboardingTooltip"
            @onboarding-tooltip-dismissed="${this.onTooltipDismissed_}">
        </contextual-tasks-onboarding-tooltip>
      ` : ''}
      <cr-composebox
          id="composebox"
          style="${this.getComposeboxBoundsStyles_()}"
          ?autofocus="${false}"
          carousel-on-top_
          entrypoint-name="ContextualTasks"
          searchbox-layout-mode="TallBottomContext"
          .lensButtonDisabled="${false}"
          .showLensButton="${this.showLensButton_}"
          .disableCaretColorAnimation="${true}"
          .isInCoBrowsingZeroState="${this.isZeroState}"
          .lensButtonTriggersOverlay="${true}"
          @result-changed="${this.onSuggestionsResultReceived_}">
      </cr-composebox>
    </div>
    ${!this.isSidePanel && this.enableNativeZeroStateSuggestions ? html`
      <cr-composebox-dropdown
          id="contextualTasksSuggestionsContainer"
          role="listbox"
          .result="${this.zeroStateSuggestions_}"
          .maxSuggestions="${5}"
          ?hidden="${!this.isZeroState}">
      </cr-composebox-dropdown>
    ` : ''}
  <!--_html_template_end_-->`;
}
// clang-format on
