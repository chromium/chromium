// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsSearchPageIndexElement} from './search_page_index.js';

export function getHtml(this: SettingsSearchPageIndexElement) {
  return html`<!--_html_template_start_-->
<cr-view-manager id="viewManager" class="cr-centered-card-container"
    ?show-all="${this.shouldShowAll}">
  <settings-search-page slot="view" id="parent"
      route-path="${this.routes_.SEARCH.path}">
  </settings-search-page>

  ${this.searchSettingsUpdateEnabled_ ? html`
    <settings-site-shortcuts-page slot="view" id="siteShortcuts">
    </settings-site-shortcuts-page>

    <settings-feature-shortcuts-page slot="view" id="featureShortcuts">
    </settings-feature-shortcuts-page>

    <settings-keyboard-shortcut-page slot="view" id="keyboardShortcut">
    </settings-keyboard-shortcut-page>
  ` : html`
    <settings-search-engines-page slot="view" id="searchEngines"
        data-parent-view-id="parent"
        route-path="${this.routes_.SEARCH_ENGINES.path}">
    </settings-search-engines-page>
  `}
</cr-view-manager>
<!--_html_template_end_-->`;
}
