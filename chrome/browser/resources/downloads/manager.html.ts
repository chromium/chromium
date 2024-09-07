// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {DownloadsManagerElement} from './manager.js';

export function getHtml(this: DownloadsManagerElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<downloads-toolbar id="toolbar" role="none" .items="${this.items_}"
    .spinnerActive="${this.spinnerActive_}"
    .hasClearableDownloads="${this.hasClearableDownloads_()}"
    @spinner-active-changed="${this.onSpinnerActiveChanged_}"
    @search-changed="${this.onSearchChanged_}">
</downloads-toolbar>
<div id="drop-shadow" class="cr-container-shadow"></div>
<div id="mainContainer" @scroll="${this.onScroll_}"
    @save-dangerous-click="${this.onSaveDangerousClick_}">
  <managed-footnote ?hidden="${this.inSearchMode_}"></managed-footnote>
  <cr-infinite-list id="downloadsList" .items="${this.items_}"
      role="grid" aria-rowcount="${this.items_.length}"
      ?hidden="${!this.hasDownloads_}" .scrollTarget="${this.listScrollTarget_}"
      .template=${(item: any, index: number, tabindex: number) => html`
  <if expr="_google_chrome">
        <downloads-item .data="${item}" tabindex="${tabindex}"
            .listTabIndex="${tabindex}" .lastFocused="${this.lastFocused_}"
            @last-focused-changed="${this.onLastFocusedChanged_}"
            .listBlurred="${this.listBlurred_}"
            @list-blurred-changed="${this.onListBlurredChanged_}"
            .focusRowIndex="${index}"
            .showEsbPromotion="${this.shouldShowEsbPromotion_(item)}"
        >
  </if>
  <if expr="not _google_chrome">
        <downloads-item .data="${item}" tabindex="${tabindex}"
            .listTabIndex="${tabindex}" .lastFocused="${this.lastFocused_}"
            @last-focused-changed="${this.onLastFocusedChanged_}"
            .listBlurred="${this.listBlurred_}"
            @list-blurred-changed="${this.onListBlurredChanged_}"
            .focusRowIndex="${index}"
        >
  </if>
        </downloads-item>`
      }>
  </cr-infinite-list>
  <div id="no-downloads" ?hidden="${this.hasDownloads_}">
    <div>
      <div class="illustration"></div>
      <span>${this.noDownloadsText_()}</span>
    </div>
  </div>
</div>
<cr-toast-manager duration="0">
  <cr-button aria-label="$i18n{undoDescription}" @click="${this.onUndoClick_}">
    $i18n{undo}
  </cr-button>
</cr-toast-manager>
${this.shouldShowBypassWarningPrompt_() ? html`
  ${this.dangerousDownloadInterstitial_ ?
    html`<downloads-dangerous-download-interstitial
        .bypassPromptItemId="${this.bypassPromptItemId_}"
        .trustSiteLine="${this.computeDangerousInterstitialTrustSiteLine_()}"
        .trustSiteLineAccessibleText=
        "${this.computeDangerInterstitialTrustSiteAccessible_()}"
        @cancel="${this.onDangerousDownloadInterstitialCancel_}"
        @close="${this.onDangerousDownloadInterstitialClose_}">
    </downloads-dangerous-download-interstitial>` :
    html`<downloads-bypass-warning-confirmation-dialog
        .fileName="${this.computeBypassWarningDialogFileName_()}"
        @close="${this.onBypassWarningConfirmationDialogClose_}">
    </downloads-bypass-warning-confirmation-dialog>`
  }` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
