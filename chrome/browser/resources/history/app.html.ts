// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {HistoryAppElement} from './app.js';

export function getHtml(this: HistoryAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    <history-query-manager
        .queryResult="${this.queryResult_}"
        @query-finished="${this.onQueryFinished_}"
        @query-state-changed="${this.onQueryStateChanged_}">
    </history-query-manager>
    <history-router id="router"
        .selectedPage="${this.selectedPage_}"
        .queryState="${this.queryState_}"
        .lastSelectedTab="${this.lastSelectedTab_}"
        @selected-page-changed="${this.onSelectedPageChanged_}">
    </history-router>
    <history-toolbar id="toolbar"
        ?has-drawer="${this.hasDrawer_}"
        ?has-more-results="${!this.queryResult_.info?.finished}"
        ?pending-delete="${this.pendingDelete_}"
        .queryInfo="${this.queryResult_.info}"
        ?querying="${this.queryState_.querying}"
        .searchTerm="${this.queryState_.searchTerm}"
        ?spinner-active="${this.shouldShowSpinner_()}"
        .selectedPage="${this.selectedPage_}"
        @search-term-native-before-input="${this.onToolbarSearchInputNativeBeforeInput_}"
        @search-term-native-input="${this.onToolbarSearchInputNativeInput_}"
        @search-term-cleared="${this.onToolbarSearchCleared_}">
    </history-toolbar>
    <div id="drop-shadow" class="cr-container-shadow"></div>
    <div id="main-container">
      <history-side-bar id="contentSideBar"
          .selectedPage="${this.selectedPage_}"
          @selected-page-changed="${this.onSelectedPageChanged_}"
          .selectedTab="${this.selectedTab_}"
          @selected-tab-changed="${this.onSelectedTabChanged_}"
          .footerInfo="${this.footerInfo}"
          ?history-clusters-enabled="${this.historyClustersEnabled_}"
          ?history-clusters-visible="${this.historyClustersVisible_}"
          @history-clusters-visible-changed="${this.onHistoryClustersVisibleChanged_}"
          ?hidden="${this.hasDrawer_}">
      </history-side-bar>
      <cr-page-selector id="content" attr-for-selected="path"
          selected="${this.contentPage_}"
          @iron-select="${this.updateScrollTarget_}">
        <div id="tabsContainer" path="history">
          <div id="historyEmbeddingsDisclaimer" class="history-cards"
              ?hidden="${!this.enableHistoryEmbeddings_}"
              ?narrow="${this.hasDrawer_}">
            <div id="historyEmbeddingsDisclaimerContent">
              $i18n{historyEmbeddingsDisclaimer}
              <a id="historyEmbeddingsDisclaimerLink"
                  href="$i18n{historyEmbeddingsSettingsUrl}" target="_blank"
                  aria-describedby="historyEmbeddingsDisclaimer"
                  @click="${this.onHistoryEmbeddingsDisclaimerLinkClick_}"
                  @auxclick="${this.onHistoryEmbeddingsDisclaimerLinkClick_}">
                $i18n{learnMore}
              </a>
            </div>
          </div>
          ${this.showTabs_ ? html`
            <div id="tabs">
              <cr-tabs .tabNames="${this.tabsNames_}"
                  .tabIcons="${this.tabsIcons_}"
                  selected="${this.selectedTab_}"
                  @selected-changed="${this.onSelectedTabChanged_}">
              </cr-tabs>
            </div>` : ''}
          <div id="tabsScrollContainer" class="cr-scrollable">
            <div class="cr-scrollable-top-shadow" ?hidden="${this.showTabs_}"></div>
            <if expr="not is_chromeos">
              ${this.shouldShowHistoryPageHistorySyncPromo_() ? html`
                <div class="history-cards">
                  <history-sync-promo></history-sync-promo>
                </div>` : ''}
            </if>
            ${this.enableHistoryEmbeddings_ ? html`
              <div id="historyEmbeddingsContainer" class="history-cards">
                <history-embeddings-promo></history-embeddings-promo>
                <cr-history-embeddings-filter-chips
                    .timeRangeStart="${this.queryStateAfterDate_}"
                    ?enable-show-results-by-group-option="${this.showHistoryClusters_}"
                    ?show-results-by-group="${this.getShowResultsByGroup_()}"
                    @show-results-by-group-changed="${this.onShowResultsByGroupChanged_}"
                    @selected-suggestion-changed="${this.onSelectedSuggestionChanged_}">
                </cr-history-embeddings-filter-chips>
                ${this.shouldShowHistoryEmbeddings_() ? html`
                  <cr-history-embeddings
                      .searchQuery="${this.queryState_.searchTerm}"
                      .timeRangeStart="${this.queryStateAfterDate_}"
                      .numCharsForQuery="${this.numCharsTypedInSearch_}"
                      @more-from-site-click="${this.onHistoryEmbeddingsItemMoreFromSiteClick_}"
                      @remove-item-click="${this.onHistoryEmbeddingsItemRemoveClick_}"
                      @is-empty-changed="${this.onHistoryEmbeddingsIsEmptyChanged_}"
                      ?force-suppress-logging="${this.historyEmbeddingsDisclaimerLinkClicked_}"
                      ?show-more-from-site-menu-option="${!this.getShowResultsByGroup_()}"
                      ?show-relative-times="${this.getShowResultsByGroup_()}"
                      ?other-history-result-clicked="${this.nonEmbeddingsResultClicked_}">
                  </cr-history-embeddings>` : ''}
              </div>` : ''}
            <cr-page-selector id="tabsContent" attr-for-selected="path"
                selected="${this.tabsContentPage_}"
                @iron-select="${this.updateScrollTarget_}">
              <history-list id="history" .queryState="${this.queryState_}"
                  ?is-active="${this.getShowHistoryList_()}"
                  searched-term="${this.queryResult_.info?.term}"
                  ?pending-delete="${this.pendingDelete_}"
                  @pending-delete-changed="${this.onListPendingDeleteChanged_}"
                  .queryResult="${this.queryResult_}"
                  path="history"
                  .scrollTarget="${this.scrollTarget_}"
                  .scrollOffset="${this.tabContentScrollOffset_}">
              </history-list>
              ${this.historyClustersSelected_() ? html`
                <history-clusters id="history-clusters"
                    ?is-active="${this.getShowResultsByGroup_()}"
                    .query="${this.queryState_.searchTerm}"
                    .timeRangeStart="${this.queryStateAfterDate_}"
                    path="grouped"
                    .scrollTarget="${this.scrollTarget_}"
                    .scrollOffset="${this.tabContentScrollOffset_}">
                </history-clusters>`: ''}
            </cr-page-selector>
          </div>
        </div>
        ${this.syncedTabsSelected_() ? html`
          <div id="syncedDevicesScroll" class="cr-scrollable" path="syncedTabs">
            <div class="cr-scrollable-top-shadow"></div>
            <history-synced-device-manager id="synced-devices"
                .sessionList="${this.sessionList_}"
                .searchTerm="${this.queryState_.searchTerm}">
            </history-synced-device-manager>
          </div>` : ''}
      </cr-page-selector>
    </div>

    <cr-lazy-render-lit id="drawer" .template='${() => html`
      <cr-drawer heading="$i18n{title}" align="$i18n{textdirection}">
        <history-side-bar id="drawer-side-bar" slot="body"
            .selectedPage="${this.selectedPage_}"
            @selected-page-changed="${this.onSelectedPageChanged_}"
            .selectedTab="${this.selectedTab_}"
            @selected-tab-changed="${this.onSelectedTabChanged_}"
            ?history-clusters-enabled="${this.historyClustersEnabled_}"
            ?history-clusters-visible="${this.historyClustersVisible_}"
            @history-clusters-visible-changed="${this.onHistoryClustersVisibleChanged_}"
            .footerInfo="${this.footerInfo}">
        </history-side-bar>
      </cr-drawer>`}'>
    </cr-lazy-render-lit>
<!--_html_template_end_-->`;
  // clang-format on
}
