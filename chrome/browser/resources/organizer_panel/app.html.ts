// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OrganizerPanelAppElement} from './app.js';

export function getHtml(this: OrganizerPanelAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-toolbar-search-field autofocus id="searchField" label="$i18n{searchTabs}"
    clear-label="$i18n{clearSearch}" search-delay="100"
    @search-changed="${this.onSearchChanged_}"
    @search-icon-clicked="${this.onSearchIconClicked_}">
  <div id="shortcut" slot="suffixElement" @click="${this.onShortcutClick_}">
    ${this.shortcut_}
  </div>
</cr-toolbar-search-field>
<organizer-list id="list" .sectionDelegates="${this.sectionDelegates_}"
    .searchQuery="${this.searchQuery_}"
    @focus-after-list="${this.onFocusAfterList_}"
    @focus-before-list="${this.onFocusBeforeList_}">
</organizer-list>
<div id="dragRegion"></div>
<!--_html_template_end_-->`;
  // clang-format on
}
