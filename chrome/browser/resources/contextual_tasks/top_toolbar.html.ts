// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
    <contextual-tasks-favicon-group id="sources"
        .urls="${this.attachedTabs.map(t => t.url.url)}"
        title="Sources" @click="${this.onSourcesClick_}">
    </contextual-tasks-favicon-group>
    <cr-icon-button @click="${this.onNewThreadClick_}"
        iron-icon="contextual_tasks:edit_square"
        title="New Thread">
    </cr-icon-button>
    <cr-icon-button @click="${this.onThreadHistoryClick_}"
        iron-icon="contextual_tasks:schedule_auto" title="Thread History">
    </cr-icon-button>
    <cr-icon-button id="more" iron-icon="cr:more-vert"
        title="More" @click="${this.onMoreClick_}">
    </cr-icon-button>
    <cr-icon-button @click="${this.onCloseButtonClick_}" iron-icon="cr:close"
        title="Close">
    </cr-icon-button>
  </div>
  <cr-lazy-render-lit id="sourcesMenu" .template="${() => html`
    <contextual-tasks-sources-menu .attachedTabs="${this.attachedTabs}">
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
        <cr-icon icon="cr:help-outline"></cr-icon>
        $i18n{help}
      </button>
    </cr-action-menu>`}">
  </cr-lazy-render-lit>
  <!--_html_template_end_-->`;
}
// clang-format on
