// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TopToolbarElement} from './top_toolbar.js';

// clang-format off
export function getHtml(this: TopToolbarElement) {
  return html`<!--_html_template_start_-->
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
        aria-label="$i18n{threadHistoryTooltip}">
    </cr-icon-button>
    <contextual-tasks-favicon-group id="sources"
        .contextInfos="${this.contextInfos}"
        title="$i18n{contextTooltip}"
        aria-label="$i18n{contextTooltip}"
        @click="${this.onSourcesClick_}"
        ?hidden="${!this.shouldShowSourcesMenuButton_()}">
    </contextual-tasks-favicon-group>
    ${this.isExpandButtonEnabled ? html`
      <cr-icon-button id="more"
        iron-icon="contextual_tasks:open_in_full_tab"
        class="no-overlap" title="$i18n{openInNewTab}"
        aria-label="$i18n{openInNewTab}"
        @click="${this.onOpenInNewTabClick_}"
        ?disabled="${!this.isAiPage}">
      </cr-icon-button>
    ` :html`
      <cr-icon-button id="more" iron-icon="cr:more-vert"
        class="no-overlap" title="$i18n{moreOptionsTooltip}"
        aria-label="$i18n{moreOptionsTooltip}"
        @click="${this.onMoreClick_}">
      </cr-icon-button>
    `}
    <cr-icon-button id="closeButton"
        @click="${this.onCloseButtonClick_}"
        iron-icon="cr:close"
        title="$i18n{closeTooltip}"
        aria-label="$i18n{closeTooltip}">
    </cr-icon-button>
  </div>
  <cr-lazy-render-lit id="sourcesMenu" .template="${() => html`
    <contextual-tasks-sources-menu .contextInfos="${this.contextInfos}">
    </contextual-tasks-sources-menu>`}">
  </cr-lazy-render-lit>
  <cr-lazy-render-lit id="menu" .template="${() => html`
    <cr-action-menu>
      <button class="dropdown-item"
          @click="${this.onOpenInNewTabClick_}"
          ?disabled="${!this.isAiPage}">
        <cr-icon icon="contextual_tasks:open_in_full_tab"></cr-icon>
        $i18n{openInNewTab}
      </button>
      <div class="dropdown-divider"></div>
      <button class="dropdown-item" @click="${this.onMyActivityClick_}">
<if expr="_google_chrome">
        <div class="cr-icon google-g-icon"></div>
</if>
<if expr="not _google_chrome">
        <cr-icon icon="cr:history"></cr-icon>
</if>
        $i18n{myActivity}
      </button>
      <button class="dropdown-item" @click="${this.onHelpClick_}">
        <cr-icon icon="contextual_tasks:feedback"></cr-icon>
        $i18n{feedback}
      </button>
    </cr-action-menu>`}">
  </cr-lazy-render-lit>
  <!--_html_template_end_-->`;
}
// clang-format on
