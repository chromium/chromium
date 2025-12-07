// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AdvancedSettingsDialogElement} from './advanced_settings_dialog.js';

export function getHtml(this: AdvancedSettingsDialogElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" @close="${this.onCloseOrCancel_}">
  <div slot="title">
    ${this.i18n('advancedSettingsDialogTitle', this.destination?.displayName || '')}
  </div>
  <div slot="body">
    <print-preview-search-box id="searchBox"
        ?hidden="${!this.hasMultipleItems_()}"
        label="$i18n{advancedSettingsSearchBoxPlaceholder}"
        .search-query="${this.searchQuery_}"
        @search-query-changed="${this.onSearchQueryChanged_}" autofocus>
    </print-preview-search-box>
    <div id="itemList" class="${this.isSearching_()}">
      ${this.getVendorCapabilities_().map(item => html`
        <print-preview-advanced-settings-item .capability="${item}">
        </print-preview-advanced-settings-item>
      `)}
    </div>
    <div class="no-settings-match-hint" ?hidden="${!this.shouldShowHint_()}">
      $i18n{noAdvancedSettingsMatchSearchHint}
    </div>
  </div>
  <div slot="button-container">
    <cr-button class="cancel-button" @click="${this.onCancelButtonClick_}">
      $i18n{cancel}
    </cr-button>
    <cr-button class="action-button" @click="${this.onApplyButtonClick_}">
      $i18n{advancedSettingsDialogConfirm}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
  // clang-format on
}
