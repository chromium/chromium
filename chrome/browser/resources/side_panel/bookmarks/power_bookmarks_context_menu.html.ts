// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PowerBookmarksContextMenuElement} from './power_bookmarks_context_menu.js';

export function getHtml(this: PowerBookmarksContextMenuElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-action-menu id="menu" @mousedown="${this.onMousedown_}"
    @focusout="${this.onFocusout_}">
  ${this.getMenuItemsForBookmarks_().map(item => html`
    ${!this.showDivider_(item) ? html`
      <button class="dropdown-item" data-id="${item.id}"
          @click="${this.onMenuItemClick_}"
          ?disabled="${!!item.disabled}">
        ${item.label}
      </button>
    ` : html`
      <hr>
    `}
  `)}
</cr-action-menu>
<!--_html_template_end_-->`;
  // clang-format on
}
