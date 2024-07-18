// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PowerBookmarkRowElement} from './power_bookmark_row.ts';

export function getHtml(this: PowerBookmarkRowElement) {
  const urlListItem = html`
<cr-url-list-item id="crUrlListItem"
    role="listitem"
    .size="${this.listItemSize}"
    .url="${this.bookmark?.url}"
    .imageUrls="${this.imageUrls}"
    .count="${this.bookmark?.children?.length}"
    .title="${this.bookmark?.title}"
    .description="${this.description}"
    .descriptionMeta="${this.getBookmarkDescriptionMeta_(this.bookmark)}"
    .itemAriaLabel="${this.rowAriaLabel}"
    .itemAriaDescription="${this.getBookmarkA11yDescription_(this.bookmark)}"
    @click="${this.onRowClicked_}"
    @auxclick="${this.onRowClicked_}"
    @contextmenu="${this.onContextMenu_}"
    ?forceHover="${this.forceHover}">

  ${this.hasCheckbox ? html`
    <cr-checkbox id="checkbox" slot="prefix" ?hidden="${!this.hasCheckbox}"
        ?checked="${this.checkboxChecked}"
        @checked-changed="${this.onCheckboxChange_}"
        ?disabled="${this.checkboxDisabled}">
      $i18n{checkboxA11yLabel}
    </cr-checkbox>` : ''}

  ${this.hasInput ? html`
    <cr-input slot="content" id="input" .value="${this.bookmark?.title}"
        class="stroked"
        @change="${this.onInputChange_}" @blur="${this.onInputBlur_}"
        @keydown="${this.onInputKeyDown_}"
        .ariaLabel="${this.rowAriaLabel}"
        .ariaDescription="${this.getBookmarkA11yDescription_(this.bookmark)}">
    </cr-input>` : ''}

  ${this.showTrailingIcon_() ? html`
    ${this.isPriceTracked_(this.bookmark) ? html`
    <sp-list-item-badge slot="badges"
        .updated="${this.showDiscountedPrice_(this.bookmark)}">
      <iron-icon icon="bookmarks:price-tracking"></iron-icon>
      <div>
        ${this.getCurrentPrice_(this.bookmark)}
      </div>
      <div slot="previous-badge"
          ?hidden="${!this.showDiscountedPrice_(this.bookmark)}">
        ${this.getPreviousPrice_(this.bookmark)}
      </div>
    </sp-list-item-badge>
  ` : ''}
    <cr-icon-button slot="suffix" .ironIcon="${this.trailingIcon}"
        ?hidden="${!this.trailingIcon}" @click="${this.onTrailingIconClicked_}"
        .title="${this.trailingIconTooltip}"
        .ariaLabel="${this.trailingIconAriaLabel}"></cr-icon-button>
  ` : ''}

  ${this.isBookmarksBar_() ? html`
    <iron-icon slot="folder-icon" icon="bookmarks:bookmarks-bar"></iron-icon>
  ` :''}

  ${this.isShoppingCollection ? html`
    <iron-icon slot="folder-icon" icon="bookmarks:shopping-collection">
        </iron-icon>` : ''}
</cr-url-list-item>`;

return (this.bookmark?.children && this.bookmark.children.length > 0 &&
    this.bookmarksTreeViewEnabled && this.compact) ? html`
<cr-expand-button no-hover id="expandButton">${urlListItem}
    </cr-expand-button>` : urlListItem;
}
