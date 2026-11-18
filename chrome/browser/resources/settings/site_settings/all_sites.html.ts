// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrInfiniteListElement} from 'chrome://resources/cr_elements/cr_infinite_list/cr_infinite_list.js';
import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {AllSitesElement} from './all_sites.js';
import {SortMethod} from './constants.js';
import type {SiteGroup} from './site_settings_browser_proxy.js';

export interface TemplatizedDomNodes {
  allSitesList: CrInfiniteListElement<SiteGroup>;
}

export function getHtml(this: AllSitesElement) {
  return html`<!--_html_template_start_-->
<settings-subpage page-title="$i18n{siteSettingsAllSites}"
    search-label="$i18n{siteSettingsAllSitesFilter}"
    search-icon="settings20:filter-list" .searchTerm="${this.filter}"
    preserve-search-term @search-term-changed="${this.onSearchTermChanged_}"
    route-path="${this.routePath}">
  <div id="sort">
    <label id="sortLabel">$i18n{siteSettingsAllSitesSort}</label>
    <select id="sortMethod" class="md-select" aria-labelledby="sortLabel"
        @change="${this.onSortMethodChange_}">
      <option value="${SortMethod.MOST_VISITED}"
          ?selected="${this.isSortMethodSelected_(SortMethod.MOST_VISITED)}">
        $i18n{siteSettingsAllSitesSortMethodMostVisited}
      </option>
      <option value="${SortMethod.STORAGE}"
          ?selected="${this.isSortMethodSelected_(SortMethod.STORAGE)}">
        $i18n{siteSettingsAllSitesSortMethodStorage}
      </option>
      <option value="${SortMethod.NAME}"
          ?selected="${this.isSortMethodSelected_(SortMethod.NAME)}">
        $i18n{siteSettingsAllSitesSortMethodName}
      </option>
    </select>
  </div>
  <div id="clearAllContainer">
    <div id="clearLabel">
      ${this.getClearStorageDescription_()}
    </div>
    <div id="clearAllButton" ?hidden="${!this.shouldShowClearAllButton_()}">
      <cr-button type="button" @click="${this.onConfirmClearAllDataClick_}">
        ${this.getClearDataButtonString_()}
      </cr-button>
    </div>
  </div>
  <div class="list-frame" ?hidden="${!this.siteGroupMapEmpty_()}">
    <div class="list-item secondary">$i18n{emptyAllSitesPage}</div>
  </div>
  <div id="noSitesFoundText" class="list-frame"
      ?hidden="${!this.noSearchResultFound_()}">
    <div class="list-item secondary">$i18n{noSitesFound}</div>
  </div>
  <div class="list-frame without-heading" id="listContainer">
    ${this.subpageScrollTarget ? html`
      <cr-infinite-list id="allSitesList" .items="${this.filteredList_}"
          .scrollTarget="${this.subpageScrollTarget}"
          @restore-list-focus="${this.onRestoreListFocus_}"
          .template="${(item: SiteGroup, index: number, tabIndex: number) =>
          html`
            <site-entry .siteGroup="${item}" .listIndex="${index}"
                .listTabIndex="${tabIndex}" .focusRowIndex="${index}"
                tabindex="${tabIndex}" .lastFocused="${this.lastFocused_}"
                @last-focused-changed="${this.onLastFocusedChanged_}"
                .listBlurred="${this.listBlurred_}"
                @list-blurred-changed="${this.onListBlurredChanged_}"
                .sortMethod="${this.sortMethod_}">
            </site-entry>
          `}">
      </cr-infinite-list>
    ` : ''}
  </div>

  <!-- Confirm remove site dialog. -->
  <cr-lazy-render-lit id="confirmRemoveSite"
      .template="${() => html`
        <cr-dialog>
          <div id="removeSiteTitle" slot="title">
            ${this.getRemoveSiteTitle_()}
          </div>
          <div slot="body">
            <div id="logoutBulletPoint" class="detail">
              <cr-icon icon="all-sites:logout-custom" aria-hidden="true"
                  role="presentation">
              </cr-icon>
              ${this.getRemoveSiteLogoutBulletPoint_()}
            </div>
            <div class="detail">
              <cr-icon icon="all-sites:offline-custom" aria-hidden="true"
                  role="presentation">
              </cr-icon>
              $i18n{siteSettingsRemoveSiteOfflineData}
            </div>
            <div id="permissionsBulletPoint" class="detail"
                ?hidden="${!this.shouldShowPermissionsBulletPoint_()}">
              <cr-icon icon="privacy:page-info"></cr-icon>
              $i18n{siteSettingsRemoveSitePermissions}
            </div>
          </div>
          <div slot="button-container">
            <cr-button class="cancel-button"
                @click="${this.onCloseDialogClick_}">
              $i18n{cancel}
            </cr-button>
            <cr-button class="action-button"
                @click="${this.onConfirmRemoveSiteClick_}">
              $i18n{siteSettingsRemoveSiteConfirm}
            </cr-button>
          </div>
        </cr-dialog>
      `}">
  </cr-lazy-render-lit>

  <!-- Confirm clear all data dialog. -->
  <cr-lazy-render-lit id="confirmClearAllData"
      .template="${() => html`
        <cr-dialog>
          <div slot="title">${this.getClearAllStorageDialogTitle_()}</div>
          <div slot="body">
            <div id="clearAllStorageDialogDescription">
              ${this.getClearAllStorageDialogDescription_()}
            </div>
            <div class="detail-list">
              <div id="clearAllStorageDialogSignOutLabel" class="detail">
                <cr-icon icon="all-sites:logout-custom" aria-hidden="true"
                    role="presentation">
                </cr-icon>
                ${this.getClearAllStorageDialogSignOutLabel_()}
              </div>
              <div class="detail">
                <cr-icon icon="all-sites:offline-custom" aria-hidden="true"
                    role="presentation">
                </cr-icon>
                $i18n{siteSettingsSiteGroupDeleteOfflineData}
              </div>
            </div>
          </div>
          <div slot="button-container">
            <cr-button class="cancel-button"
                @click="${this.onCloseDialogClick_}">
              $i18n{cancel}
            </cr-button>
            <cr-button class="action-button"
                @click="${this.onClearAllDataClick_}">
              $i18n{siteSettingsSiteClearStorage}
            </cr-button>
          </div>
        </cr-dialog>
      `}">
  </cr-lazy-render-lit>
</settings-subpage>
<!--_html_template_end_-->`;
}
