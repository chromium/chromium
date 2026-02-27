// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {HistoryClustersAppElement} from './app.js';

export function getHtml(this: HistoryClustersAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-toolbar-search-field id="searchbox"
    @search-changed="${this.onSearchChanged_}"
    label="$i18n{historyClustersSearchPrompt}"
    clear-label="$i18n{clearSearch}"
    @contextmenu="${this.onContextmenu_}"
    icon-override="${this.searchIcon_}"
    @search-term-native-input="${this.onSearchTermNativeInput_}"
    @search-term-cleared="${this.onSearchTermCleared_}">
</cr-toolbar-search-field>
${this.enableHistoryEmbeddings_ ? html`
<div id="historyEmbeddingsDisclaimer">
  $i18n{historyEmbeddingsDisclaimer}
  <a id="historyEmbeddingsDisclaimerLink" href="#"
      aria-describedby="historyEmbeddingsDisclaimer"
      @click="${this.onHistoryEmbeddingsDisclaimerLinkClick_}"
      @auxclick="${this.onHistoryEmbeddingsDisclaimerLinkAuxclick_}">
    $i18n{learnMore}
  </a>
</div>
` : ''}
<div id="embeddingsScrollContainer"
    class="sp-scroller sp-scroller-bottom-of-page">
  ${this.shouldShowHistoryEmbeddingsResults_() ? html`
  <cr-history-embeddings show-relative-times in-side-panel
      .searchQuery="${this.query}"
      ?force-suppress-logging="${this.historyEmbeddingsDisclaimerLinkClicked_}"
      ?other-history-result-clicked="${this.nonEmbeddingsResultClicked_}"
      .numCharsForQuery="${this.numCharsTypedInSearch_}"
      @answer-click="${this.onHistoryEmbeddingsAnswerClick_}"
      @answer-context-menu="${this.onAnswerContextMenu_}"
      @is-empty-changed="${this.onHistoryEmbeddingsIsEmptyChanged_}"
      @result-click="${this.onHistoryEmbeddingsResultClick_}"
      @result-context-menu="${this.onHistoryEmbeddingsResultContextMenu_}"
      @remove-item-click="${this.onHistoryEmbeddingsRemoveItemClick_}">
  </cr-history_embeddings>
  ` : ''}
  <history-clusters id="historyClusters"
      query="${this.query}"
      path="journeys"
      @query-changed-by-user="${this.onQueryChangedByUser_}"
      @record-history-link-click="${this.onClusterRecordHistoryLinkClick_}"
      class="${this.getClustersComponentClass_()}"
      .scrollTarget="${this.scrollTarget_}">
  </history-clusters>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
