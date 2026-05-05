// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TopToolbarElement} from './top_toolbar.js';

// clang-format off
export function getHtml(this: TopToolbarElement) {
  return html`<!--_html_template_start_-->
<div id="top-row">
<if expr="_google_chrome">
    <img src="chrome://resources/cr_components/searchbox/icons/google_g_gradient.svg"
        class="top-toolbar-logo">
</if>
<if expr="not _google_chrome">
    <img class="top-toolbar-logo chrome-logo-light"
        src="chrome://resources/cr_components/searchbox/icons/chrome_product.svg"
        alt="Chrome Logo">
    <img class="top-toolbar-logo chrome-logo-dark"
        src="chrome://resources/images/chrome_logo_dark.svg" alt="Chrome Logo">
</if>
  <div class="top-toolbar-title">
    ${this.title}
  </div>
  <div class="top-toolbar-action-buttons">
    <cr-icon-button id="pinButton"
        @click="${this.onPinClick_}"
        iron-icon="${this.isPinned ?
            'contextual_tasks:keep' : 'contextual_tasks:keep_off'}"
        title="${this.getPinButtonTooltip_()}"
        aria-label="${this.getPinButtonTooltip_()}"
        ?hidden="${!this.shouldShowPinButton_()}">
    </cr-icon-button>
    <cr-icon-button id="newThreadButton"
        @click="${this.onNewThreadClick_}"
        iron-icon="contextual_tasks:edit_square"
        class="no-overlap" title="$i18n{newThreadTooltip}"
        aria-label="$i18n{newThreadTooltip}">
    </cr-icon-button>
    <cr-icon-button id="threadHistoryButton"
        @click="${this.onThreadHistoryClick_}"
        iron-icon="contextual_tasks:notes_spark"
        class="no-overlap" title="$i18n{threadHistoryTooltip}"
        aria-label="$i18n{threadHistoryTooltip}"
        ?hidden="${!this.isAiPage}">
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
        iron-icon="contextual_tasks:open_in_full_tab"
        class="no-overlap" title="$i18n{openInNewTab}"
        aria-label="$i18n{openInNewTab}"
        @click="${this.onOpenInNewTabClick_}"
        ?disabled="${!this.enableOpenInNewTabButton}">
      </cr-icon-button>
    ` : html`
      <cr-icon-button id="overflowMenuButton" iron-icon="cr:more-vert"
        class="no-overlap" title="$i18n{moreOptionsTooltip}"
        aria-label="$i18n{moreOptionsTooltip}"
        @click="${this.onOverflowMenuButtonClick_}"
        ?hidden="${this.hideOverflowMenuButton_}">
      </cr-icon-button>
    `}
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
      .enableOpenInNewTabButton="${this.enableOpenInNewTabButton}">
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
