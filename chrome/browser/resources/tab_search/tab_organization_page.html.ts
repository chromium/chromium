// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TabOrganizationPageElement} from './tab_organization_page.js';
import {TabOrganizationState} from './tab_search.mojom-webui.js';

export function getHtml(this: TabOrganizationPageElement) {
  return html`<!--_html_template_start_-->
<div id="contents">
  <div id="body">
    <tab-organization-not-started id="notStarted"
        ?shown="${this.isState_(TabOrganizationState.kNotStarted)}"
        @sign-in-click="${this.onSignInClick_}"
        @organize-tabs-click="${this.onOrganizeTabsClick_}"
        @learn-more-click="${this.onLearnMoreClick_}"
        ?show-fre="${this.showFRE_}">
    </tab-organization-not-started>
    <tab-organization-in-progress id="inProgress"
        ?shown="${this.isState_(TabOrganizationState.kInProgress)}">
    </tab-organization-in-progress>
    <tab-organization-results id="results"
        ?shown="${this.isState_(TabOrganizationState.kSuccess)}"
        .session="${this.session_}"
        ?multi-tab-organization="${this.multiTabOrganization_}"
        available-height="${this.availableHeight_}"
        @reject-click="${this.onRejectClick_}"
        @reject-all-groups-click="${this.onRejectAllGroupsClick_}"
        @create-group-click="${this.onCreateGroupClick_}"
        @create-all-groups-click="${this.onCreateAllGroupsClick_}"
        @remove-tab="${this.onRemoveTab_}"
        @learn-more-click="${this.onLearnMoreClick_}"
        @feedback="${this.onFeedback_}">
    </tab-organization-results>
    <tab-organization-failure id="failure"
        ?shown="${this.isState_(TabOrganizationState.kFailure)}"
        ?show-fre="${this.showFRE_}"
        .error="${this.getSessionError_()}"
        @check-now="${this.onCheckNow_}"
        @tip-click="${this.onTipClick_}">
    </tab-organization-failure>
  </div>
</div><!--_html_template_end_-->`;
}
