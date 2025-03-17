// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {AppState} from './app.js';
import type {ProductSpecificationsElement} from './app.js';

// clang-format off
export function getHtml(this: ProductSpecificationsElement) {
  return html`<!--_html_template_start_-->
  <div id="appContainer">
    <product-specifications-header id="header" .subtitle="${this.setName_}"
        ?disabled="${!this.isHeaderEnabled_()}"
        ?is-page-title-clickable="${this.id_ !== null}"
        @delete-click="${this.onHeaderMenuDeleteClick_}"
        @name-change="${this.updateSetName_}"
        @see-all-click="${this.seeAllSets_}">
    </product-specifications-header>

    <div id="contentContainer"
        ?hidden="${this.appState_ === AppState.NO_CONTENT}">
      <div id="summaryContainer">
        <div id="tableDataUnavailable"
            ?hidden="${!this.showTableDataUnavailableContainer_}">
          <picture>
            <source srcset="images/empty_state_dark.svg"
                media="(prefers-color-scheme: dark)">
            <img id="emptyImg" src="images/empty_state.svg">
          </picture>
          <div id="empty"
              class="table-data-unavailable-container"
              ?hidden="${this.appState_ !== AppState.TABLE_EMPTY}">
            <div class="table-data-unavailable-message">
              $i18n{emptyStateTitle}
            </div>
            <div class="table-data-unavailable-description">
              $i18n{emptyStateDescription}
            </div>
            <product-selector id="productSelector"
                @selected-url-change="${this.onUrlAdd_}">
            </product-selector>
          </div>
          <div id="syncPromo"
              class="table-data-unavailable-container"
              ?hidden="${this.appState_ !== AppState.SYNC_SCREEN}">
            <div class="table-data-unavailable-message">
              $i18n{compareSyncMessage}
            </div>
            <div class="table-data-unavailable-description">
              $i18n{compareSyncDescription}
            </div>
            <cr-button id="turnOnSyncButton" class="action-button"
                @click="${this.showSyncSetupFlow_}">
              $i18n{compareSyncButton}
            </cr-button>
          </div>
          <div id="error"
              class="table-data-unavailable-container"
              ?hidden="${this.appState_ !== AppState.ERROR}">
            <div class="table-data-unavailable-message">
              $i18n{compareErrorMessage}
            </div>
            <div class="table-data-unavailable-description">
              $i18n{compareErrorDescription}
            </div>
          </div>
        </div>
        <loading-state id="loading"
            ?hidden="${this.appState_ !== AppState.LOADING}"
            .columnCount="${this.loadingState_.urlCount}">
        </loading-state>
        <horizontal-carousel id="specs"
            ?hidden="${this.appState_ !== AppState.TABLE_POPULATED}">
            <product-specifications-table slot="table"
                id="summaryTable"
                .columns="${this.tableColumns_}"
                @url-change="${this.onUrlChange_}"
                @url-remove="${this.onUrlRemove_}"
                @url-order-update="${this.onUrlOrderUpdate_}"
                @unavailable-action-attempted="${this.showOfflineToast_}">
              <div slot="selectorGradient" id="selectorGradient"></div>
              <new-column-selector slot="newColumnSelector"
                  id="newColumnSelector"
                  .excludedUrls="${this.getTableUrls_()}"
                  .isTableFull="${this.isTableFull_()}"
                  @selected-url-change="${this.onUrlAdd_}">
              </new-column-selector>
            </product-specifications-table>
        </horizontal-carousel>
      </div>

      <comparison-table-list id="comparisonTableList"
          .tables="${this.sets_}"
          @item-click="${this.onComparisonTableListItemClick_}"
          @rename-table="${this.onComparisonTableListItemRename_}"
          ?hidden="${!this.showComparisonTableList_}">
      </comparison-table-list>
    </div>

    <div id="footer"
        ?hidden="${!this.canShowFooter_(this.showTableDataUnavailableContainer_,
            this.appState_)}">
      <div id="disclaimer">${this.getDisclaimerText_()}
        <a id="learnMoreLink" href="$i18n{compareLearnMoreUrl}"
            target="_blank" aria-label="$i18n{learnMoreA11yLabel}">
          $i18n{learnMore}
        </a>
      </div>
      ${this.canShowFeedbackButtons_() ? html`
        <div id="feedbackLoading" ?hidden="${this.appState_ !== AppState.LOADING}">
          <cr-loading-gradient>
            <svg height="16" width="44" xmlns="http://www.w3.org/2000/svg">
              <clipPath>
                <circle cx="8" cy="8" r="8"></circle>
                <circle cx="36" cy="8" r="8"></circle>
              </clipPath>
            </svg>
          </cr-loading-gradient>
        </div>
        <cr-feedback-buttons id="feedbackButtons"
            ?hidden="${this.loadingState_.loading}"
            @selected-option-changed="${this.onFeedbackSelectedOptionChanged_}">
        </cr-feedback-buttons>
      ` : ''}
    </div>
  </div>

  <cr-toast id="offlineToast">
    <div>$i18n{offlineMessage}</div>
  </cr-toast>
  <cr-toast id="errorToast" duration="10000">
    <div>$i18n{errorMessage}</div>
  </cr-toast>
  <!--_html_template_end_-->`;
}
// clang-format on
