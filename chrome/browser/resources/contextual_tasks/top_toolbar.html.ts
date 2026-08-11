// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {getHtml as getLogoHtml} from './top_toolbar_logo.html.js';
import type {TopToolbarElement} from './top_toolbar.js';

// clang-format off
export function getHtml(this: TopToolbarElement) {
  return html`<!--_html_template_start_-->
<div id="top-row" data-element-id="kContextualTasksWebUIToolbarElementId">
  <if expr="not is_android">
    ${this.isPermissionShowing_() ? html`
      <permission-dashboard
          .dashboardState="${this.permissionDashboardState}">
      </permission-dashboard>
    ` : ''}
  </if>
  <div class="top-toolbar-logo-container"
      ?hidden="${this.isPermissionShowing_()}">
    ${this.isSidePanelRearchitectureEnabled_ ? html`
      <cr-button class="top-toolbar-logo-button clickable"
          data-element-id="kContextualTasksSuperGButtonElementId"
          @click="${this.onLogoClick_}">
        ${getLogoHtml()}
      </cr-button>
    ` : html`
      <div class="top-toolbar-logo-button">
        ${getLogoHtml()}
      </div>
    `}
  </div>
  <div class="top-toolbar-title">
    ${this.title}
  </div>
  <div class="top-toolbar-action-buttons">
    <cr-icon-button id="newThreadButton"
        @click="${this.onNewThreadClick_}"
        iron-icon="${this.webuiRoundedIconsEnabled_
            ? 'contextual_tasks:edit-square'
            : 'contextual_tasks:edit_square-old'}"
        class="no-overlap" title="$i18n{newThreadTooltip}"
        aria-label="$i18n{newThreadTooltip}"
        ?hidden="${!this.isAimEligible ||
            (this.contextualTasksEnableSpatialModelToolbarLayout_ &&
             this.contextualTasksEnableSpatialModelToolbarLayoutNewThreadInOverflow_)}">
    </cr-icon-button>
    <cr-icon-button id="threadHistoryButton"
        @click="${this.onThreadHistoryClick_}"
        iron-icon="${this.webuiRoundedIconsEnabled_
            ? 'contextual_tasks:notes-spark'
            : 'contextual_tasks:notes_spark-old'}"
        class="no-overlap" title="$i18n{threadHistoryTooltip}"
        aria-label="$i18n{threadHistoryTooltip}"
        ?hidden="${!this.isAiPage || !this.isUserSignedIn ||
            this.contextualTasksEnableSpatialModelToolbarLayout_}">
    </cr-icon-button>

    ${!this.contextManagementInComposeboxEnabled_ ? html`
    <contextual-tasks-favicon-group id="sources"
        .contextInfos="${this.contextInfos}"
        title="$i18n{contextTooltip}"
        aria-label="$i18n{contextTooltip}"
        @click="${this.onSourcesClick_}"
        ?hidden="${!this.shouldShowSourcesMenuButton_()}">
    </contextual-tasks-favicon-group>` : ''}
    ${this.isExpandButtonEnabled ? html`
      <cr-icon-button id="openInNewTabButton"
        iron-icon="${this.webuiRoundedIconsEnabled_
            ? 'contextual_tasks:open-in-full'
            : 'contextual_tasks:open_in_full_tab-old'}"
        class="no-overlap" title="$i18n{openInNewTab}"
        aria-label="$i18n{openInNewTab}"
        @click="${this.onOpenInNewTabClick_}"
        ?disabled="${!this.enableOpenInNewTabButton}">
      </cr-icon-button>
    ` : ''}
    <cr-icon-button id="overflowMenuButton" iron-icon="cr:more-vert"
      data-element-id="kContextualTasksWebUIOverflowMenuElementId"
      class="no-overlap ${this.overflowMenuOpen_ ? 'active' : ''}" title="$i18n{moreOptionsTooltip}"
      aria-label="$i18n{moreOptionsTooltip}"
      @click="${this.onOverflowMenuButtonClick_}"
      ?hidden="${this.hideOverflowMenuButton_}">
    </cr-icon-button>
    <cr-icon-button id="closeButton"
        @click="${this.onCloseButtonClick_}"
        iron-icon="cr:close"
        title="$i18n{closeTooltip}"
        aria-label="$i18n{closeTooltip}"
        rounded-corner="${this.isExpandButtonEnabled ? 'false' : 'true'}">
    </cr-icon-button>
  </div>
</div>
  <cr-lazy-render-lit id="sourcesMenu" .template="${() => html`
    <contextual-tasks-sources-menu .contextInfos="${this.contextInfos}">
    </contextual-tasks-sources-menu>`}">
  </cr-lazy-render-lit>
  <cr-lazy-render-lit id="overflowMenu" .template="${() => html`
    <contextual-tasks-overflow-menu
        .enableOpenInNewTabButton="${this.enableOpenInNewTabButton}"
        .isPinned="${this.isPinned}"
        .isPinButtonEnabled="${this.isPinButtonEnabled}"
        .isAiPage="${this.isAiPage}"
        .isAimEligible="${this.isAimEligible}"
        .contextualTasksEnableSpatialModelToolbarLayout="${this.contextualTasksEnableSpatialModelToolbarLayout_}"
        .contextualTasksEnableSpatialModelToolbarLayoutNewThreadInOverflow="${this.contextualTasksEnableSpatialModelToolbarLayoutNewThreadInOverflow_}"
        @pin-click="${this.onPinClick_}"
        @new-thread-click="${this.onNewThreadClick_}"
        @open-changed="${this.onOverflowMenuOpenChanged_}">
    </contextual-tasks-overflow-menu>`}">
  </cr-lazy-render-lit>
  ${this.showReopenTabs_ ? html`
    <reopen-tabs
        @reopen-click="${this.onReopenTabsReopenClick_}"
        @dismiss-click="${this.onReopenTabsDismissClick_}">
    </reopen-tabs>` : ''}
  <!--_html_template_end_-->`;
}
// clang-format on
