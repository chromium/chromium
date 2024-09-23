// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ReadingListAppElement} from './reading_list_app.js';

export function getHtml(this: ReadingListAppElement) {
  return html`<!--_html_template_start_-->
<div id="content" ?hidden="${this.loadingContent_}">
  <sp-empty-state
      ?hidden="${!this.isReadingListEmpty_()}"
      image-path="./images/read_later_empty.svg"
      dark-image-path="./images/read_later_empty_dark.svg"
      heading="$i18n{emptyStateHeader}"
      body="${this.getEmptyStateSubheaderText_()}">
  </sp-empty-state>
  <div id="readingListList" class="sp-scroller sp-scroller-top-of-page"
      @keydown="${this.onItemKeyDown_}"
      ?hidden="${!this.shouldShowList_()}">
    <div class="sp-card" ?hidden="${!this.unreadItems_.length}">
      <sp-heading compact hide-back-button>
        <h2 slot="heading">$i18n{unreadHeader}</h2>
      </sp-heading>
      ${this.unreadItems_.map((item, index) => html`
        <reading-list-item data-url="${item.url.url}" data-index="${index}"
            @focus="${this.onItemFocus_}"
            aria-label="${this.ariaLabel_(item)}" class="unread-item"
            .data="${item}" ?button-ripples="${this.buttonRipples}">
        </reading-list-item>
      `)}
    </div>
    <div class="sp-cards-separator" ?hidden="${!this.shouldShowHr_()}"></div>
    <div class="sp-card" ?hidden="${!this.readItems_.length}">
      <sp-heading compact hide-back-button>
        <h2 slot="heading">$i18n{readHeader}</h2>
      </sp-heading>
      ${this.readItems_.map((item, index) => html`
        <reading-list-item data-url="${item.url.url}" data-index="${index}"
            @focus="${this.onItemFocus_}"
            aria-label="${this.ariaLabel_(item)}"
            .data="${item}" ?button-ripples="${this.buttonRipples}">
        </reading-list-item>
      `)}
    </div>
  </div>
  <sp-footer ?pinned="${!this.isReadingListEmpty_()}">
    <cr-button id="currentPageActionButton" class="floating-button"
        aria-label="${this.getCurrentPageActionButtonText_()}"
        @click="${this.onCurrentPageActionButtonClick_}"
        ?disabled="${this.getCurrentPageActionButtonDisabled_()}">
      <cr-icon id="currentPageActionButtonIcon" aria-hidden="true"
          slot="prefix-icon"
          icon="${this.getCurrentPageActionButtonIcon_()}">
      </cr-icon>
      <div id="currentPageActionButtonText" aria-hidden="true">
        ${this.getCurrentPageActionButtonText_()}
      </div>
    </cr-button>
  </sp-footer>
</div>
<!--_html_template_end_-->`;
}
