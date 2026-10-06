// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OrganizerListSectionElement} from './organizer_list_section.js';

export function getHtml(this: OrganizerListSectionElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<organizer-list-section-header id="header" ?expanded="${this.isExpanded_()}"
    ?disabled="${this.isSearching_()}"
    @expanded-changed="${this.onExpandedChanged_}"
    @show-all-changed="${this.onShowAllChanged_}">
  ${this.delegate?.getHeader() || ''}
</organizer-list-section-header>
${this.hasNoSearchResults_() ? html`
  <div id="noResults">$i18n{noResults}</div>
` : html`
  <cr-collapse id="items" role="list" ?opened="${this.isExpanded_()}"
      ?no-animation="${this.noAnimation_}">
    ${this.hasZeroState_() ? this.getZeroState_() : ''}
    ${this.getFilteredItems_().map(item => html`
      <organizer-list-section-item .item="${item}" role="listitem"
          @click="${this.onItemClick_}"
          @action-button-click="${this.onItemActionButtonClick_}"
          @context-menu-click="${this.onItemContextMenuClick_}">
      </organizer-list-section-item>
    `)}
  </cr-collapse>
`}
<!--_html_template_end_-->`;
  // clang-format on
}
