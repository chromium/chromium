// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {OmniboxPopupAppElement} from './app.js';
import {getHtml as getContextualEntrypointHtml} from './app_contextual_entrypoint.html.js';

export function getHtml(this: OmniboxPopupAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.showContextEntrypoint_ ? html`
<!-- WebUI Omnibox popup w/ "Add Context" button -->
<div class="dropdownContainer">
  ${this.searchboxLayoutMode_ === 'TallTopContext' ? html`
    ${getContextualEntrypointHtml.call(this)}
    ${this.hasVisibleMatches_ ?
      html`<div class="carousel-divider"></div>` : nothing}
  ` : nothing}
  <cr-searchbox-dropdown part="searchbox-dropdown"
      exportparts="dropdown-content"
      role="listbox" .result="${this.result_}"
      ?can-show-secondary-side="${this.canShowSecondarySide}"
      ?has-secondary-side="${this.hasSecondarySide}"
      @has-secondary-side-changed="${this.onHasSecondarySideChanged_}"
      @dom-change="${this.onResultRepaint_}"
      ?hidden="${!this.hasVisibleMatches_}">
  </cr-searchbox-dropdown>
  ${this.searchboxLayoutMode_ !== 'TallTopContext' ?
    getContextualEntrypointHtml.call(this) : nothing}
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
