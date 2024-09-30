// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AutoTabGroupsPageElement} from './auto_tab_groups_page.js';
import {TabOrganizationState} from '../tab_search.mojom-webui.js';

export function getHtml(this: AutoTabGroupsPageElement) {
  return html`<!--_html_template_start_-->
<div id="contents">
  <div id="body">
    <div id="header"
        class="auto-tab-groups-header"
        aria-live="polite"
        aria-relevant="all">
      ${
      this.showBackButton ? html`
        <cr-icon-button class="back-button"
            iron-icon="cr:arrow-back"
            @click="${this.onBackClick_}">
        </cr-icon-button>
      ` :
                            ''}
      ${this.getTitle_()}
    </div>
    <auto-tab-groups-not-started id="notStarted"
        ?shown="${this.isState_(TabOrganizationState.kNotStarted)}"
        model-strategy="${this.modelStrategy_}"
        @model-strategy-change="${this.onModelStrategyChange_}"
        @sign-in-click="${this.onSignInClick_}"
        @organize-tabs-click="${this.onOrganizeTabsClick_}"
        @learn-more-click="${this.onLearnMoreClick_}"
        ?show-fre="${this.showFRE_}">
    </auto-tab-groups-not-started>
    <auto-tab-groups-in-progress id="inProgress"
        ?shown="${this.isState_(TabOrganizationState.kInProgress)}">
    </auto-tab-groups-in-progress>
    <auto-tab-groups-results id="results"
        ?shown="${this.isState_(TabOrganizationState.kSuccess)}"
        .session="${this.session_}"
        ?multi-tab-organization="${this.multiTabOrganization_}"
        available-height="${this.availableHeight_}"
        @name-change="${this.onNameChange_}"
        @reject-click="${this.onRejectClick_}"
        @reject-all-groups-click="${this.onRejectAllGroupsClick_}"
        @create-group-click="${this.onCreateGroupClick_}"
        @create-all-groups-click="${this.onCreateAllGroupsClick_}"
        @remove-tab="${this.onRemoveTab_}"
        @learn-more-click="${this.onLearnMoreClick_}"
        @feedback="${this.onFeedback_}">
    </auto-tab-groups-results>
    <auto-tab-groups-failure id="failure"
        ?shown="${this.isState_(TabOrganizationState.kFailure)}"
        ?show-fre="${this.showFRE_}"
        .error="${this.getSessionError_()}"
        @check-now="${this.onCheckNow_}"
        @tip-click="${this.onTipClick_}">
    </auto-tab-groups-failure>
  </div>
</div><!--_html_template_end_-->`;
}
