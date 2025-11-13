// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {HistoryListElement} from './history_list.js';

export function getHtml(this: HistoryListElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    <div id="noResults" class="centered-message"
        ?hidden="${this.hasResults_()}">
      ${this.noResultsMessage_()}
    </div>

    <cr-infinite-list id="infiniteList" class="history-cards"
        .items="${this.historyData_}"
        item-size="36" chunk-size="50"
        role="grid" aria-rowcount="${this.historyData_.length}"
        ?hidden="${!this.hasResults_()}"
        .scrollTarget="${this.scrollTarget}" .scrollOffset="${this.scrollOffset}"
        .template='${(item: any, index: number, tabindex: number) => html`
            <history-item tabindex="${tabindex}"
                .item="${item}"
                ?selected="${item.selected}"
                ?is-card-start="${this.isCardStart_(item, index)}"
                ?is-card-end="${this.isCardEnd_(item, index)}"
                ?has-time-gap="${this.needsTimeGap_(item, index)}"
                .searchTerm="${this.searchedTerm}"
                .numberOfItems="${this.historyData_.length}"
                .index="${index}"
                .focusRowIndex="${index}"
                .listTabIndex="${tabindex}"
                .lastFocused="${this.lastFocused_}"
                @last-focused-changed="${this.onLastFocusedChanged_}"
                .listBlurred="${this.listBlurred_}"
                @list-blurred-changed="${this.onListBlurredChanged_}">
            </history-item>`}'>
    </cr-infinite-list>

    <cr-lazy-render-lit id="dialog" .template='${() => html`
        <cr-dialog consume-keydown-event>
          <div slot="title" id="title">$i18n{removeSelected}</div>
          <div slot="body" id="body">$i18n{deleteWarning}</div>
          <div slot="button-container">
            <cr-button class="cancel-button" @click="${this.onDialogCancelClick_}">
              $i18n{cancel}
            </cr-button>
<if expr="is_macosx">
            <cr-button class="action-button" @click="${this.onDialogConfirmClick_}"
                aria-describedby="title body">
              $i18n{deleteConfirm}
            </cr-button>
</if>
<if expr="not is_macosx">
            <cr-button class="action-button" @click="${this.onDialogConfirmClick_}">
              $i18n{deleteConfirm}
            </cr-button>
</if>
          </div>
        </cr-dialog>`}'>
    </cr-lazy-render-lit>

    <cr-lazy-render-lit id="sharedMenu" .template='${() => html`
        <cr-action-menu role-description="$i18n{actionMenuDescription}">
          <button id="menuMoreButton" class="dropdown-item"
              ?hidden="${!this.canSearchMoreFromSite_()}"
              @click="${this.onMoreFromSiteClick_}">
            $i18n{moreFromSite}
          </button>
          <button id="menuRemoveButton" class="dropdown-item"
              ?hidden="${!this.canDeleteHistory_}"
              ?disabled="${this.pendingDelete}"
              @click="${this.onRemoveFromHistoryClick_}">
            $i18n{removeFromHistory}
          </button>
          <button id="menuRemoveBookmarkButton" class="dropdown-item"
              ?hidden="${!this.actionMenuModel_?.item.starred}"
              @click="${this.onRemoveBookmarkClick_}">
            $i18n{removeBookmark}
          </button>
        </cr-action-menu>`}'>
    </cr-lazy-render-lit>
<!--_html_template_end_-->`;
  // clang-format on
}
