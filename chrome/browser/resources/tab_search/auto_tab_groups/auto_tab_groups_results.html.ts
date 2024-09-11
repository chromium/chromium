// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AutoTabGroupsResultsElement} from './auto_tab_groups_results.js';

export function getHtml(this: AutoTabGroupsResultsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="auto-tab-groups-container">
  <div id="scrollable">
    ${this.getOrganizations_().map(item => html`
      <auto-tab-groups-group
        name="${this.getName_(item)}"
        .tabs="${item.tabs}"
        first-new-tab-index="${item.firstNewTabIndex}"
        organization-id="${item.organizationId}"
        ?multi-tab-organization="${this.multiTabOrganization}"
        ?show-reject="${this.hasMultipleOrganizations_()}">
      </auto-tab-groups-group>
    `)}
  </div>
  ${this.multiTabOrganization ? html`
    <auto-tab-groups-results-actions show-clear
        ?multiple-organizations="${this.hasMultipleOrganizations_()}"
        @create-group-click="${this.onCreateAllGroupsClick_}">
    </auto-tab-groups-results-actions>
  ` : ''}
  <div class="feedback" role="toolbar" @keydown="${this.onFeedbackKeyDown_}">
    <div class="button-row">
      <div class="auto-tab-groups-body">
        $i18n{learnMoreDisclaimer1}
      </div>
      <cr-feedback-buttons id="feedbackButtons"
          tabindex="-1"
          selected-option="${this.feedbackSelectedOption_}"
          @selected-option-changed="${this.onFeedbackSelectedOptionChanged_}">
      </cr-feedback-buttons>
    </div>
    <div class="auto-tab-groups-body">
      $i18n{learnMoreDisclaimer2}
      <div id="learnMore" class="auto-tab-groups-link"
          @click="${this.onLearnMoreClick_}"
          @keydown="${this.onLearnMoreKeyDown_}"
          role="link"
          tabindex="0"
          aria-label="$i18n{learnMoreAriaLabel}">
        $i18n{learnMore}
      </div>
    </div>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
