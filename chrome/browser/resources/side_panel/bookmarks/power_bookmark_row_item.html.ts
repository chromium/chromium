// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PowerBookmarkRowItemElement} from './power_bookmark_row_item.ts';

export function getHtml(this: PowerBookmarkRowItemElement) {
  // clang-format off
return html`<!--_html_template_start_-->
<cr-url-list-item id="crUrlListItem"
    role="treeitem"
    aria-level="${this.depth + 1}"
    .size="${this.listItemSize}"
    .url="${this.getUrl_()}"
    ?selected="${this.isSelected}"
    .imageUrls="${this.getBookmarkImageUrls_()}"
    .count="${this.bookmark.children?.length}"
    .title="${this.bookmark.title}"
    .description="${this.getBookmarkDescription_(this.bookmark)}"
    .descriptionMeta="${this.getBookmarkDescriptionMeta_()}"
    .itemAriaLabel="${this.getBookmarkA11yLabel_()}"
    .itemAriaDescription="${this.getBookmarkA11yDescription_()}"
    @click="${this.onClick_}"
    @auxclick="${this.onAuxclick_}"
    @contextmenu="${this.onContextmenu_}"
    ?force-hover="${this.getBookmarkForceHover_()}">

  ${this.hasCheckbox ? html`
    <cr-checkbox id="checkbox" slot="prefix"
        ?checked="${this.isCheckboxChecked_()}"
        @checked-changed="${this.onCheckboxCheckedChanged_}"
        ?disabled="${!this.canEdit_()}">
      $i18n{checkboxA11yLabel}
    </cr-checkbox>` : ''}

  ${this.isRenamingItem_() ? html`
    <cr-input slot="content" id="input" .value="${this.bookmark.title}"
        class="stroked"
        @change="${this.onInputChange_}" @blur="${this.onInputBlur_}"
        @keydown="${this.onInputKeydown_}"
        .ariaLabel="${this.getBookmarkA11yLabel_()}"
        .ariaDescription="${this.getBookmarkA11yDescription_()}">
    </cr-input>` : ''}

  ${this.showTrailingIcon_() ? html`
    ${this.isPriceTracked ? html`
    <sp-list-item-badge slot="badges"
        ?was-updated="${this.showDiscountedPrice_()}">
      <cr-icon icon="bookmarks:price-tracking"></cr-icon>
      <div>${this.getCurrentPrice_(this.bookmark)}</div>
      <div slot="previous-badge" ?hidden="${!this.showDiscountedPrice_()}">
        ${this.getPreviousPrice_(this.bookmark)}
      </div>
    </sp-list-item-badge>
  ` : ''}
    <cr-icon-button slot="suffix" iron-icon="cr:more-vert"
        @click="${this.onTrailingIconClick_}"
        .title="${this.trailingIconTooltip}"
        .ariaLabel="${this.getBookmarkMenuA11yLabel_()}">
    </cr-icon-button>
  ` : ''}

  ${this.isBookmarksBar_() ? html`
    <cr-icon class="bookmark-icon" slot="folder-icon"
        icon="bookmarks:bookmarks-bar"></cr-icon>
  ` :''}

  ${this.isShoppingCollection_() ? html`
    <cr-icon slot="folder-icon" icon="bookmarks:shopping-collection">
    </cr-icon>
  ` : ''}
</cr-url-list-item>
<!--_html_template_end_-->`;
}
// clang-format off

