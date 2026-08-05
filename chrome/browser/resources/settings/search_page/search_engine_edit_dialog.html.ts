// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsSearchEngineEditDialogElement} from './search_engine_edit_dialog.js';

export function getHtml(this: SettingsSearchEngineEditDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" close-text="$i18n{close}">
  <div slot="title">${this.dialogTitle_}</div>
  <div slot="body" spellcheck="false">
    ${this.showPolicySubtitle_ ? html`
      <div id="policySubtitleContainer">
        <cr-icon icon="cr:domain"></cr-icon>
        <span class="secondary">
          $i18n{searchEnginesDeleteConfirmationSubtitleForPolicy}
        </span>
      </div>
    ` : ''}
    <cr-input id="searchEngine"
        label="$i18n{searchEnginesName}"
        ?readonly="${this.readonly_}"
        error-message="$i18n{notValid}"
        .value="${this.searchEngine_}"
        @value-changed="${this.onSearchEngineValueChanged_}"
        @input="${this.onSearchEngineInput_}"
        autofocus>
    </cr-input>
    <cr-input id="keyword"
        label="$i18n{searchEnginesShortcut}"
        ?readonly="${this.readonly_}"
        error-message="$i18n{notValid}"
        .value="${this.keyword_}"
        @value-changed="${this.onKeywordValueChanged_}"
        @focus="${this.onKeywordFocus_}"
        @input="${this.onKeywordInput_}">
    </cr-input>
    <cr-input id="queryUrl"
        label="$i18n{searchEnginesQueryURLExplanation}"
        ?readonly="${this.urlIsReadonly_}"
        error-message="$i18n{notValid}"
        .value="${this.queryUrl_}"
        @value-changed="${this.onQueryUrlValueChanged_}"
        @focus="${this.onQueryUrlFocus_}"
        @input="${this.onQueryUrlInput_}">
    </cr-input>
  </div>
  <div slot="button-container">
    <cr-button class="cancel-button" @click="${this.onCancelClick_}"
        id="cancel" ?hidden="${this.readonly_}">
      $i18n{cancel}
    </cr-button>
    <cr-button id="actionButton" class="action-button"
        @click="${this.onActionButtonClick_}">
      ${this.actionButtonText_}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
