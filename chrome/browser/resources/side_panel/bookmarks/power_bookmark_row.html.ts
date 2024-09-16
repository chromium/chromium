// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PowerBookmarkRowElement} from './power_bookmark_row.ts';

export function getHtml(this: PowerBookmarkRowElement) {
  const { id, url, title, children } = this.bookmark || {};
  // clang-format off
  const urlListItem = html`
<cr-url-list-item id="crUrlListItem"
    role="listitem"
    .size="${this.listItemSize}"
    .url="${url}"
    .imageUrls="${this.getBookmarkImageUrls_(this.bookmark)}"
    .count="${children?.length}"
    .title="${title}"
    .description="${this.getBookmarkDescription_(this.bookmark)}"
    .descriptionMeta="${this.getBookmarkDescriptionMeta_(this.bookmark)}"
    .itemAriaLabel="${this.getBookmarkA11yLabel_(url,title)}"
    .itemAriaDescription="${this.getBookmarkA11yDescription_(this.bookmark)}"
    @click="${this.onRowClicked_}"
    @auxclick="${this.onRowClicked_}"
    @contextmenu="${this.onContextMenu_}"
    .forceHover="${this.getBookmarkForceHover_(this.bookmark)}">

  ${this.hasCheckbox ? html`
    <cr-checkbox id="checkbox" slot="prefix"
        ?checked="${this.isCheckboxChecked_()}"
        @checked-changed="${this.onCheckboxChange_}"
        ?disabled="${!this.canEdit_(this.bookmark)}">
      $i18n{checkboxA11yLabel}
    </cr-checkbox>` : ''}

  ${this.renamingItem_(id) ? html`
    <cr-input slot="content" id="input" .value="${title}"
        class="stroked"
        @change="${this.onInputChange_}" @blur="${this.onInputBlur_}"
        @keydown="${this.onInputKeyDown_}"
        .ariaLabel="${this.getBookmarkA11yLabel_(url,title)}"
        .ariaDescription="${this.getBookmarkA11yDescription_(this.bookmark)}">
    </cr-input>` : ''}

  ${this.showTrailingIcon_() ? html`
    ${this.isPriceTracked_(this.bookmark) ? html`
    <sp-list-item-badge slot="badges"
        .updated="${this.showDiscountedPrice_(this.bookmark)}">
      <cr-icon icon="bookmarks:price-tracking"></cr-icon>
      <div>
        ${this.getCurrentPrice_(this.bookmark)}
      </div>
      <div slot="previous-badge"
          ?hidden="${!this.showDiscountedPrice_(this.bookmark)}">
        ${this.getPreviousPrice_(this.bookmark)}
      </div>
    </sp-list-item-badge>
  ` : ''}
    <cr-icon-button slot="suffix" iron-icon="cr:more-vert"
        @click="${this.onTrailingIconClicked_}"
        .title="${this.trailingIconTooltip}"
        .ariaLabel="${this.getBookmarkMenuA11yLabel_(url, title!)}">
    </cr-icon-button>
  ` : ''}

  ${this.isBookmarksBar_() ? html`
    <cr-icon slot="folder-icon" icon="bookmarks:bookmarks-bar"></cr-icon>
  ` :''}

  ${this.isShoppingCollection_(this.bookmark) ? html`
    <cr-icon slot="folder-icon" icon="bookmarks:shopping-collection">
    </cr-icon>
  ` : ''}
</cr-url-list-item>`;

if (this.shouldExpand_()) {
  return html`
<cr-expand-button no-hover id="expandButton"
    @expanded-changed=${this.onExpandedChanged_}>
  ${urlListItem}
</cr-expand-button>
  ${this.toggleExpand ? html`
    ${children!.map((item: chrome.bookmarks.BookmarkTreeNode)=> html`
      <power-bookmark-row
          id="bookmark-${item.id}"
          .bookmark="${item}"
          .compact="${this.compact}"
          .depth="${this.depth + 1}"
          trailingIconTooltip="$i18n{tooltipMore}"
          .hasCheckbox="${this.hasCheckbox}"
          .renamingId="${this.renamingId}"
          .imageUrls="${this.imageUrls}"
          .shoppingCollectionFolderId="${this.shoppingCollectionFolderId}"
          .bookmarksService="${this.bookmarksService}"
          .contextMenuBookmark="${this.contextMenuBookmark}">
      </power-bookmark-row>
    `)}`: ''
  }`;
} else {
    return html`
    ${this.compact && this.bookmarksTreeViewEnabled ? html`
      <div id="bookmark">
        ${urlListItem}
      </div>` : urlListItem
    }`;
  }
}
// clang-format off
