// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsSearchPageElement} from './search_page.js';

export function getHtml(this: SettingsSearchPageElement) {
  return html`<!--_html_template_start_-->
${this.settingsRefresh2026Enabled_ ? html`
  <h2 class="section-header">${this.searchPageTitle_}</h2>
  <div id="headerDescription" class="secondary">
    $i18n{searchEngineChoiceEntryPointSubtitle}
    <a href="$i18n{searchExplanationLearnMoreURL}"
        aria-description="$i18n{opensInNewTab}" target="_blank">
      $i18n{searchExplanationLearnMore}
    </a>
  </div>
` : ''}
<settings-section
    page-title="${
        this.settingsRefresh2026Enabled_ ? '' : this.searchPageTitle_}">
  <div route-path="default">
    <!-- Omnibox search engine -->
    <div class="cr-row first">
      <div class="default-search-engine flex">
        ${!this.settingsRefresh2026Enabled_ ? html`
          ${this.searchPageTitle_}
          <div class="secondary">
            $i18n{searchEngineChoiceEntryPointSubtitle}
            <a href="$i18n{searchExplanationLearnMoreURL}"
                aria-description="$i18n{opensInNewTab}"
                aria-label="$i18n{searchExplanationLearnMoreA11yLabel}"
                target="_blank">
              $i18n{learnMore}
            </a>
          </div>
        ` : ''}
        <div class="cr-row first">
          <settings-search-engine-icon .engine="${this.defaultSearchEngine_}">
          </settings-search-engine-icon>
          <div class="search-engine-name">
            ${this.defaultSearchEngine_?.name}
          </div>
          ${this.isDefaultSearchControlledByPolicy_() ? html`
            <cr-policy-pref-indicator
                .pref="${this.defaultSearchProviderDataPref_}">
            </cr-policy-pref-indicator>
          ` : ''}
          <cr-button id="openDialogButton"
              @click="${this.onOpenDialogButtonClick_}"
              ?disabled="${this.isDefaultSearchEngineEnforced_()}">
            $i18n{searchEnginesChange}
          </cr-button>
        </div>
      </div>
    </div>
    ${this.defaultSearchProviderDataPref_?.extensionId ? html`
      ${this.searchSettingsUpdateEnabled_ ? html`
        <div class="cr-row">
          <extension-controlled-message class="flex"
              extension-id="${
                  this.defaultSearchProviderDataPref_?.extensionId}"
              extension-name="${
                  this.defaultSearchProviderDataPref_?.controlledByName}"
              ?extension-can-be-disabled="${
                  !!this.defaultSearchProviderDataPref_
                        ?.extensionCanBeDisabled}">
          </extension-controlled-message>
        </div>
      ` : html`
        <div class="cr-row continuation">
          <extension-controlled-indicator class="flex"
              extension-id="${
                  this.defaultSearchProviderDataPref_?.extensionId}"
              extension-name="${
                  this.defaultSearchProviderDataPref_?.controlledByName}"
              ?extension-can-be-disabled="${
                  !!this.defaultSearchProviderDataPref_
                        ?.extensionCanBeDisabled}">
          </extension-controlled-indicator>
        </div>
      `}
    ` : ''}

    ${!this.searchSettingsUpdateEnabled_ ? html`
      <!-- Manage search engines -->
      <cr-link-row class="hr" id="enginesSubpageTrigger"
          label="$i18n{searchEnginesManageSiteSearch}"
          @click="${this.onManageSearchEnginesClick_}"
          role-description="$i18n{subpageArrowRoleDescription}">
      </cr-link-row>
    ` : ''}
  </div>
</settings-section>
${this.showSearchEngineListDialog_ ? html`
  <settings-search-engine-list-dialog .searchEngines="${this.searchEngines_}"
      .saveGuestChoice="${this.saveGuestChoice_}"
      @close="${this.onSearchEngineListDialogClose_}"
      @more-engines-click="${this.onMoreEnginesClick_}"
      @save-guest-choice-changed="${this.onSaveGuestChoiceChanged_}"
      @search-engine-changed="${this.onSearchEngineChanged_}">
  </settings-search-engine-list-dialog>
` : ''}
${this.showMoreEnginesDialog_ ? html`
  <settings-search-engine-more-engines-dialog
      .saveGuestChoice="${this.saveGuestChoice_}"
      @back-click="${this.onMoreEnginesBackClick_}"
      @close="${this.onMoreEnginesDialogClose_}"
      @save-guest-choice-changed="${this.onSaveGuestChoiceChanged_}"
      @search-engine-changed="${this.onSearchEngineChanged_}">
  </settings-search-engine-more-engines-dialog>
` : ''}
<cr-toast id="confirmationToast" duration="10000">
  <div>${this.confirmationToastLabel_}</div>
</cr-toast>
<!--_html_template_end_-->`;
}
