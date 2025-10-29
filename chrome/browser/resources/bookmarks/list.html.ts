// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {BookmarksListElement} from './list.js';

export function getHtml(this: BookmarksListElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<if expr="not is_chromeos">
  <promo-card id="promoCard" class="card"
      ?hidden="${!this.shouldShowPromoCard_}"
      @on-should-show-promo-card="${this.updateShouldShowPromoCard_}">
  </promo-card>
</if>
<cr-lazy-list id="list" class="card"
    .items="${this.displayedIds_}"
    .scrollTarget="${this}"
    item-size="40" chunk-size="10"
    ?hidden="${this.isEmptyList_()}"
    role="grid"
    no-restore-focus
    aria-label="$i18n{listAxLabel}"
    aria-multiselectable="true"
    .template="${(id: string, index: number) => html`
    <bookmarks-item data-index="${index}"
        id="bookmark_${index}"
        item-id="${id}"
        draggable="true"
        role="row"
        tabindex="${this.getTabindex_(index)}"
        .ironListTabIndex="${this.getTabindex_(index)}"
        @keydown="${this.onItemKeydown_}"
        @focus="${this.onItemFocus_}"
        aria-rowindex="${this.getAriaRowindex_(index)}"
        aria-selected="${this.getAriaSelected_(id)}">
    </bookmarks-item>`}">
</cr-lazy-list>
<div id="message" class="centered-message" ?hidden="${!this.isEmptyList_()}">
  ${this.emptyListMessage_()}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
