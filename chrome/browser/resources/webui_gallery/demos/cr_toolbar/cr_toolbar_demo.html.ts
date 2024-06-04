// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrToolbarDemoElement} from './cr_toolbar_demo.js';

export function getHtml(this: CrToolbarDemoElement) {
  return html`
<cr-toolbar
    .pageName="${this.pageName_}"
    .searchPrompt="${this.searchPrompt_}"
    .clearLabel="${this.clearLabel_}"
    .menuLabel="${this.menuLabel_}"
    ?narrow="${this.narrow_}"
    @narrow-changed="${this.onNarrowChanged_}"
    .narrowThreshold="${this.narrowThreshold_}"
    ?always-show-logo="${this.alwaysShowLogo_}"
    ?show-menu="${this.showMenu_}"
    ?show-search="${this.showSearch_}"
    @cr-toolbar-menu-click="${this.onMenuClick_}"
    @search-changed="${this.onSearchChanged_}">
  <div ?hidden="${!this.showSlottedContent_}">
    Slotted right-hand content
  </div>
</cr-toolbar>

<div class="content">
  <h1>cr-toolbar</h1>
  <div class="demos">
    <cr-input label="Page name" .value="${this.pageName_}"
        @value-changed="${this.onPageNameChanged_}"></cr-input>
    <cr-input label="Search prompt" .value="${this.searchPrompt_}"
        @value-changed="${this.onSearchPromptChanged_}"></cr-input>
    <cr-input label="Clear label" .value="${this.clearLabel_}"
        @value-changed="${this.onClearLabelChanged_}"></cr-input>
    <cr-input label="Menu label" .value="${this.menuLabel_}"
        @value-changed="${this.onMenuLabelChanged_}"></cr-input>
    <cr-input label="Max window width for narrow mode"
        .value="${this.narrowThreshold_}"
        @value-changed="${this.onNarrowThresholdChanged_}"></cr-input>
    <cr-checkbox ?checked="${this.alwaysShowLogo_}"
        @checked-changed="${this.onAlwaysShowLogoChanged_}">
      Always show logo
    </cr-checkbox>
    <cr-checkbox ?checked="${this.showMenu_}"
        @checked-changed="${this.onShowMenuChanged_}">
      Show menu button
    </cr-checkbox>
    <cr-checkbox ?checked="${this.showSearch_}"
        @checked-changed="${this.onShowSearchChanged_}">
      Show search input
    </cr-checkbox>
    <cr-checkbox ?checked="${this.showSlottedContent_}"
        @checked-changed="${this.onShowSlottedContentChanged_}">
      Show right-hand content
    </cr-checkbox>
  </div>

  <div class="log">
    <div>Is narrow? ${this.narrow_}</div>
    ${this.log_.map(item => html`<div>${item}</div>`)}
  </div>
</div>`;
}
