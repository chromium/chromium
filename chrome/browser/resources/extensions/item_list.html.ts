// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {asyncMap} from './async_map_directive.js';
import type {ExtensionsItemListElement} from './item_list.js';

export function getHtml(this: ExtensionsItemListElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="container">
  <managed-footnote ?hidden="${this.filter}"></managed-footnote>
  <div id="content-wrapper" .style="--max-columns: ${this.maxColumns_};">
    ${this.showSafetyCheckReviewPanel_ ? html`
      <div class="items-container panel">
        <extensions-review-panel .extensions="${this.unsafeExtensions_}"
            .delegate="${this.delegate}">
        </extensions-review-panel>
      </div>` : ''}

    ${this.shouldShowMv2DeprecationPanel_() ? html`
      <div class="items-container panel">
        <extensions-mv2-deprecation-panel
            .extensions="${this.mv2DeprecatedExtensions_}"
            .delegate="${this.delegate}"
            .mv2ExperimentStage="${this.mv2ExperimentStage_}"
            ?show-title="${this.showSafetyCheckReviewPanel_}">
        </extensions-mv2-deprecation-panel>
      </div>` : ''}

    <div id="no-items" class="empty-list-message"
        ?hidden="${!this.shouldShowEmptyItemsMessage_()}">
      <span @click="${this.onNoExtensionsClick_}">
        $i18nRaw{noExtensionsOrApps}
      </span>
    </div>
    <div id="no-search-results" class="empty-list-message"
        ?hidden="${!this.shouldShowEmptySearchMessage_()}">
      <span>$i18n{noSearchResults}</span>
    </div>

    <div id="extensions-section" ?hidden="${!this.shownExtensionsCount_}">
      <!-- section-header needs to left-align with the grid content below, and
           the easiest way to achieve this is to make it a grid as well. -->
      <h2 class="section-header items-container">
        $i18n{extensionsSectionHeader}
      </h2>
      <div class="items-container">
        <!-- Render only a few items first, to improve initial render time,
             then render the remaining items on a different frame. Value of 6
             was chosen by experimentation, and it is a good trade-off between
             initial render time and total render time. -->
        ${asyncMap(this.filteredExtensions_, item => html`
          <extensions-item id="${item.id}" .data="${item}"
              ?safety-check-showing="${
                  this.hasSafetyCheckTriggeringExtension_()}"
              .delegate="${this.delegate}" ?in-dev-mode="${this.inDevMode}"
              .mv2ExperimentStage="${this.mv2ExperimentStage_}">
          </extensions-item>`, 6)}
      </div>
    </div>

    <div id="chrome-apps-section" ?hidden="${!this.shownAppsCount_}">
      <!-- section-header needs to left-align with the grid content below, and
           the easiest way to achieve this is to make it a grid as well. -->
      <h2 class="section-header items-container">$i18n{appsTitle}</h2>
      <div class="items-container">
        ${asyncMap(this.filteredApps_, item => html`
          <extensions-item id="${item.id}" .data="${item}"
              .delegate="${this.delegate}" ?in-dev-mode="${this.inDevMode}">
          </extensions-item>`, 6)}
      </div>
    </div>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
