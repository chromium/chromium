// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OrganizerPanelAppElement} from './app.js';

export function getHtml(this: OrganizerPanelAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-toolbar-search-field id="searchField" label="$i18n{searchTabs}"
    clear-label="$i18n{clearSearch}" @search-changed="${this.onSearchChanged_}">
  <div id="shortcut" slot="suffixElement">${this.shortcut_}</div>
</cr-toolbar-search-field>
<organizer-list id="list" .sectionDelegates="${this.sectionDelegates_}"
    .searchQuery="${this.searchQuery_}">
</organizer-list>
<!--_html_template_end_-->`;
  // clang-format on
}
