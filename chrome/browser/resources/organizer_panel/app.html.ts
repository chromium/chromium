// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OrganizerPanelAppElement} from './app.js';

export function getHtml(this: OrganizerPanelAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-toolbar-search-field autofocus="true" id="searchField"
    label="$i18n{searchTabs}" clear-label="$i18n{clearSearch}"
    @search-changed="${this.onSearchChanged_}">
  <div id="shortcut" slot="suffixElement">${this.shortcut_}</div>
</cr-toolbar-search-field>
<organizer-list id="list" .sectionDelegates="${this.sectionDelegates_}"
    .searchQuery="${this.searchQuery_}">
</organizer-list>
<cr-action-menu id="menu">
  ${this.allSectionsExpanded_ ? html`
    <button id="collapseAllButton" class="dropdown-item"
        ?disabled="${this.isSearching_()}" @click="${this.onCollapseAllClick_}">
      $i18n{collapseAll}
    </button>
  ` : html`
    <button id="expandAllButton" class="dropdown-item"
        ?disabled="${this.isSearching_()}" @click="${this.onExpandAllClick_}">
      $i18n{expandAll}
    </button>
  `}
  <button id="closeTabSearchButton" class="dropdown-item"
      @click="${this.onCloseTabSearchClick_}">
    $i18n{closeTabSearch}
  </button>
</cr-action-menu>
<!--_html_template_end_-->`;
  // clang-format on
}
