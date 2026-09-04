// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrInfiniteListElement} from '//resources/cr_elements/cr_infinite_list/cr_infinite_list.js';
import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {HistoryEntry} from 'chrome://resources/cr_components/history/history.mojom-webui.js';

import type {HistoryListElement} from './history_list.js';

export interface TemplatizedDomNodes {
  infiniteList: CrInfiniteListElement<HistoryEntry>;
}

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
        @restore-list-focus="${this.onRestoreListFocus_}"
        .template='${(item: HistoryEntry, index: number, tabindex: number) =>
            html`
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
              </history-item>
            `}'>
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
        <cr-action-menu auto-close-on-focusout
            role-description="$i18n{actionMenuDescription}">
          <button id="menuMoreButton" class="dropdown-item"
              ?hidden="${!this.canSearchMoreFromSite_()}"
              @click="${this.onMoreFromSiteClick_}">
            $i18n{moreFromSite}
          </button>
          <button id="menuGoToGeminiChatButton" class="dropdown-item"
              ?hidden="${!this.canShowGoToGeminiChat_()}"
              @click="${this.onGoToGeminiChatClick_}">
            $i18n{goToGeminiChat}
          </button>
          <button id="menuReviewGeminiActivityButton" class="dropdown-item"
              ?hidden="${!this.canShowReviewGeminiActivity_()}"
              @click="${this.onReviewGeminiActivityClick_}">
            $i18n{reviewGeminiActivity}
          </button>
          <div class="hr"
              ?hidden="${!this.canShowReviewGeminiActivity_() &&
                         !this.canShowGoToGeminiChat_()}"></div>
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

    <cr-toast id="errorToast" duration="5000">
      $i18n{goToGeminiChatError}
    </cr-toast>
<!--_html_template_end_-->`;
  // clang-format on
}
