// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {BookmarksToolbarElement} from './toolbar.js';

export function getHtml(this: BookmarksToolbarElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-toolbar page-name="$i18n{title}"
    ?has-overlay="${this.showSelectionOverlay}"
    clear-label="$i18n{clearSearch}"
    search-prompt="$i18n{searchPrompt}"
    ?narrow="${this.narrow_}" @narrow-changed="${this.onNarrowChanged_}"
    autofocus always-show-logo
    @search-changed="${this.onSearchChanged_}">
  <cr-icon-button iron-icon="cr:more-vert"
      id="menuButton"
      title="$i18n{organizeButtonTitle}"
      @click="${this.onMenuButtonOpenClick_}"
      aria-haspopup="menu">
  </cr-icon-button>
</cr-toolbar>
<cr-toolbar-selection-overlay ?show="${this.showSelectionOverlay}"
    cancel-label="$i18n{cancel}"
    selection-label="${this.getItemsSelectedString_()}"
    @clear-selected-items="${this.onClearSelectionClick_}">
  <cr-button @click="${this.onDeleteSelectionClick_}"
      ?disabled="${!this.canDeleteSelection_()}">
    $i18n{delete}
  </cr-button>
</cr-toolbar-selection-overlay>
<!--_html_template_end_-->`;
  // clang-format on
}
