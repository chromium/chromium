// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OmniboxPopupAppElement} from './app.js';

export function getHtml(this: OmniboxPopupAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.showContextEntrypoint_ ? html`
<!-- WebUI Omnibox popup w/ "Add Context" button -->
<div class="dropdownContainer">
  <contextual-entrypoint-and-carousel id="context"
      part="contextual-entrypoint-and-carousel"
      exportparts="composebox-entrypoint, context-menu-entrypoint-icon"
      .showMenuOnClick="${false}"
      entrypoint-name="Omnibox"
      searchbox-layout-mode="${this.searchboxLayoutMode_}"
      .recentTabForChip="${this.recentTabForChip_}"
      ?hide-entrypoint-button="${this.shouldHideEntrypointButton_}"
      ?show-dropdown="${this.hasVisibleMatches_}"
      ?show-lens-search-chip="${
        this.isContentSharingEnabled_ && this.isLensSearchEligible_}"
      ?show-recent-tab-chip="${
        this.isContentSharingEnabled_ && this.computeShowRecentTabChip_()}"
      .inputState="${this.inputState_}"
      ?show-model-picker="${this.usePecApi_}"
      @add-tab-context="${this.addTabContext_}"
      @context-menu-entrypoint-click="${this.onContextualEntryPointClicked_}"
      @lens-search-click="${this.onLensSearchChipClicked_}">
    <cr-searchbox-dropdown part="searchbox-dropdown"
        exportparts="dropdown-content"
        role="listbox" .result="${this.result_}"
        ?can-show-secondary-side="${this.canShowSecondarySide}"
        ?has-secondary-side="${this.hasSecondarySide}"
        @has-secondary-side-changed="${this.onHasSecondarySideChanged_}"
        @dom-change="${this.onResultRepaint_}"
        ?hidden="${!this.hasVisibleMatches_}">
    </cr-searchbox-dropdown>
  </contextual-entrypoint-and-carousel>
</div>` : html`
<!-- WebUI Omnibox popup w/o "Add Context" button -->
  <cr-searchbox-dropdown part="searchbox-dropdown"
      exportparts="dropdown-content"
      role="listbox" .result="${this.result_}"
      ?can-show-secondary-side="${this.canShowSecondarySide}"
      ?has-secondary-side="${this.hasSecondarySide}"
      @has-secondary-side-changed="${this.onHasSecondarySideChanged_}"
      @dom-change="${this.onResultRepaint_}"
      ?hidden="${!this.hasVisibleMatches_}">
  </cr-searchbox-dropdown>
`}
<!--_html_template_end_-->`;
  // clang-format on
}
