// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {HistoryToolbarElement} from './history_toolbar.js';

export function getHtml(this: HistoryToolbarElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-toolbar id="mainToolbar"
    disable-right-content-grow
    ?has-overlay="${this.itemsSelected_}"
    page-name="$i18n{title}"
    clear-label="$i18n{clearSearch}"
    search-icon-override="${this.computeSearchIconOverride_()}"
    search-input-aria-description="${this.computeSearchInputAriaDescriptionOverride_()}"
    search-prompt="${this.computeSearchPrompt_()}"
    ?spinner-active="${this.spinnerActive}"
    autofocus
    ?show-menu="${this.hasDrawer}"
    menu-label="$i18n{historyMenuButton}"
    narrow-threshold="1023"
    @search-changed="${this.onSearchChanged_}">
</cr-toolbar>
<cr-toolbar-selection-overlay ?show="${this.itemsSelected_}"
    cancel-label="$i18n{cancel}"
    selection-label="${this.numberOfItemsSelected_(this.count)}"
    @clear-selected-items="${this.clearSelectedItems}">

  <cr-button
      @click="${this.openSelectedItems}" ?disabled="${this.pendingDelete}">
    $i18n{openSelected}
  </cr-button>

  <cr-button
      @click="${this.deleteSelectedItems}" ?disabled="${this.pendingDelete}">
    $i18n{delete}
  </cr-button>

</cr-toolbar-selection-overlay>
<!--_html_template_end_-->`;
  // clang-format on
}
