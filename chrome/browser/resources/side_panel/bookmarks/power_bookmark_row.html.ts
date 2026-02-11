// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PowerBookmarkRowElement} from './power_bookmark_row.ts';

export function getHtml(this: PowerBookmarkRowElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.shouldExpand_() ? html`
  <cr-expand-button no-hover id="expandButton"
      .expanded="${this.toggleExpand}"
      aria-expanded="${this.toggleExpand}"
      tab-index="-1"
      collapse-icon="cr:expand-more"
      ?selected="${this.isSelected}"
      expand-icon="cr:chevron-right"
      @expanded-changed="${this.onExpandedChanged_}">
    <power-bookmark-row-item id="listItem"
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
        .isSelected="${this.isSelected}"
        .selectedBookmarks="${this.selectedBookmarks}"
        .renamingId="${this.renamingId}"
        .hasFolders="${this.hasFolders}"
        .hasActiveDrag="${this.hasActiveDrag}">
    </power-bookmark-row-item>
  </cr-expand-button>
  ${this.toggleExpand ? html`
    ${this.sortedChildren.map(item => html`
      <power-bookmark-row
          id="bookmark-${item.id}"
          .bookmark="${item}"
          ?compact="${this.compact}"
          ?selected="${this.isSelected}"
          .depth="${this.depth + 1}"
          trailing-icon-tooltip="$i18n{tooltipMore}"
          ?has-checkbox="${this.hasCheckbox}"
          .selectedBookmarks="${this.selectedBookmarks}"
          .renamingId="${this.renamingId}"
          .imageUrls="${this.imageUrls}"
          .shoppingCollectionFolderId="${this.shoppingCollectionFolderId}"
          .draggable="${String(this.canDrag)}"
          ?can-drag="${this.canDrag}"
          ?has-active-drag="${this.hasActiveDrag}"
          .activeFolderPath="${this.activeFolderPath}"
          .contextMenuBookmark="${this.contextMenuBookmark}"
          .activeSortIndex="${this.activeSortIndex}"
          ?has-folders="${true}">
      </power-bookmark-row>
    `)}
  `: ''}
` : html`
  <div id="${this.getWrapperId_()}">
    <power-bookmark-row-item id="listItem"
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
        .isSelected="${this.isSelected}"
        .selectedBookmarks="${this.selectedBookmarks}"
        .renamingId="${this.renamingId}"
        .hasFolders="${this.hasFolders}"
        .hasActiveDrag="${this.hasActiveDrag}">
    </power-bookmark-row-item>
  </div>
`}
<!--_html_template_end_-->`;
  // clang-format on
}
