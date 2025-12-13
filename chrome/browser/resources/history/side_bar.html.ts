// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {HistorySideBarElement} from './side_bar.js';

export function getHtml(this: HistorySideBarElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-menu-selector id="menu" selected="${this.selectedPage}"
    @selected-changed="${this.onSelectorSelectedChanged_}"
    selectable=".page-item" attr-for-selected="path"
    @iron-activate="${this.onSelectorActivate_}"
    selected-attribute="selected">
  <a id="history" role="menuitem" class="page-item cr-nav-menu-item"
      href="${this.getHistoryItemHref_()}"
      path="${this.getHistoryItemPath_()}"
      @click="${this.onItemClick_}">
    <cr-icon icon="cr:history"></cr-icon>
    $i18n{historyMenuItem}
    <cr-ripple></cr-ripple>
  </a>
  <a id="syncedTabs" role="menuitem" href="/syncedTabs"
      class="page-item cr-nav-menu-item"
      path="syncedTabs" @click="${this.onItemClick_}">
    <cr-icon icon="cr:phonelink"></cr-icon>
    $i18n{openTabsMenuItem}
    <cr-ripple></cr-ripple>
  </a>
  <a role="menuitem" id="clear-browsing-data"
      class="cr-nav-menu-item"
      href="chrome://settings/clearBrowserData"
      @click="${this.onClearBrowsingDataClick_}"
      ?disabled="${this.guestSession_}"
      title="$i18n{clearBrowsingDataLinkTooltip}">
    <cr-icon icon="cr:delete"></cr-icon>
    $i18n{clearBrowsingData}
    <div class="cr-icon icon-external"></div>
    <cr-ripple></cr-ripple>
  </a>
</cr-menu-selector>
<div id="spacer"></div>
<div id="footer" ?hidden="${!this.showFooter_}">
  <div class="separator"></div>
  <managed-footnote></managed-footnote>
  <div id="google-account-footer"
      ?hidden="${!this.showGoogleAccountFooter_}"
      @click="${this.onGoogleAccountFooterClick_}">
    <cr-icon icon="cr:info-outline"></cr-icon>
    <div ?hidden="${!this.showGMAOnly_}">$i18nRaw{sidebarFooterGMAOnly}</div>
    <div ?hidden="${!this.showGAAOnly_}">$i18nRaw{sidebarFooterGAAOnly}</div>
    <div ?hidden="${!this.showGMAAndGAA_}">$i18nRaw{sidebarFooterGMAAndGAA}</div>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
