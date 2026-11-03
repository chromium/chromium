// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OrganizerListSectionElement} from './organizer_list_section.js';

export function getHtml(this: OrganizerListSectionElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<organizer-list-section-header id="header" ?expanded="${this.isExpanded_()}"
    ?disabled="${this.isSearching_()}" ?show-all="${this.showAll_}"
    .title="${this.delegate?.getHeader() || ''}"
    @expanded-changed="${this.onExpandedChanged_}"
    @menu-action="${this.onMenuAction_}"
    @show-all-changed="${this.onShowAllChanged_}"
    @toggle-action="${this.onToggleAction_}">
</organizer-list-section-header>
<cr-collapse id="items" role="list" ?opened="${this.isExpanded_()}"
    ?no-animation="${this.noAnimation_}">
  ${this.hasZeroState_() ? this.getZeroState_() : ''}
  ${this.getFilteredItems_().map(item => html`
    <organizer-list-section-item .item="${item}" role="listitem"
        ?has-context-menu="${this.hasContextMenu_()}"
        @click="${this.onItemClick_}"
        @action-button-click="${this.onItemActionButtonClick_}"
        @context-menu-click="${this.onItemContextMenuClick_}">
    </organizer-list-section-item>
  `)}
</cr-collapse>
<!--_html_template_end_-->`;
  // clang-format on
}
