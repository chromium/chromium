// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PowerBookmarkRowElement} from './power_bookmark_row.ts';

export function getHtml(this: PowerBookmarkRowElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.rowHeading ? html`
  <sp-heading hide-back-button>
    <h1 slot="heading">${this.rowHeading}</h1>
  </sp-heading>
` : ''}
<power-bookmark-row-item id="listItem"
    class="${this.getListItemCssClass_()}"
    .isExpandable="${this.canExpand_()}"
    .expanded="${this.isExpanded_()}"
    @expanded-changed="${this.onExpandedChanged_}"
    .bookmark="${this.bookmark}"
    ?compact="${this.compact}"
    .contextMenuBookmark="${this.contextMenuBookmark}"
    .depth="${this.depth}"
    ?has-checkbox="${this.hasCheckbox}"
    .imageUrls="${this.imageUrls}"
    .isPriceTracked="${this.isPriceTracked}"
    .searchQuery="${this.searchQuery}"
    .shoppingCollectionFolderId="${this.shoppingCollectionFolderId}"
    .trailingIconTooltip="${this.trailingIconTooltip}"
    .listItemSize="${this.listItemSize}"
    .selectedBookmarks="${this.selectedBookmarks}"
    .renamingId="${this.renamingId}"
    .hasFolders="${this.hasFolders}"
    .hasActiveDrag="${this.hasActiveDrag}">
</power-bookmark-row-item>
<!--_html_template_end_-->`;
  // clang-format on
}
