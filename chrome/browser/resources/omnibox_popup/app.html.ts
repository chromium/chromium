// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OmniboxPopupAppElement} from './app.js';

export function getHtml(this: OmniboxPopupAppElement) {
  // clang-format off
  const searchboxDropdown = html`
<cr-searchbox-dropdown part="searchbox-dropdown"
    exportparts="dropdown-content"
    role="listbox" .result="${this.result_}"
    ?can-show-secondary-side="${this.canShowSecondarySide}"
    ?has-secondary-side="${this.hasSecondarySide}"
    @has-secondary-side-changed="${this.onHasSecondarySideChanged_}"
    @dom-change="${this.onResultRepaint_}"
    ?hidden="${!this.hasVisibleMatches_}">
</cr-searchbox-dropdown>
  `;

  return html`<!--_html_template_start_-->
${this.showContextEntrypoint_ ? html`
<!-- WebUI Omnibox popup w/ "Add Context" button -->
<div class="dropdownContainer">
  <contextual-entrypoint-and-carousel id="context"
      part="contextual-entrypoint-and-carousel"
      exportparts="composebox-entrypoint, context-menu-entrypoint-icon"
      entrypoint-name="Omnibox"
      searchbox-layout-mode="${this.searchboxLayoutMode_}"
      ?show-dropdown="${this.hasVisibleMatches_}"
      ?show-lens-search-chip="${this.isLensSearchEligible_}"
      @context-menu-entrypoint-click="${this.onContextualEntryPointClicked_}">
    ${searchboxDropdown}
  </contextual-entrypoint-and-carousel>
</div>` : html`
<!-- WebUI Omnibox popup w/o "Add Context" button -->
  ${searchboxDropdown}`}
<!--_html_template_end_-->`;
  // clang-format on
}
