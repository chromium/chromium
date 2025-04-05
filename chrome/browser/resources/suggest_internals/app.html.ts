// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AppElement} from './app.js';

export function getHtml(this: AppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-toolbar page-name="Suggest Debug Tool" search-prompt="Filter requests"
    clear-label="Clear filter" @search-changed="${this.onFilterChanged_}"
    always-show-logo show-search show-menu
    @cr-toolbar-menu-click="${this.showOutputControls_}">
</cr-toolbar>
<cr-drawer id="drawer" heading="Output controls">
  <div slot="body">
    <cr-button title="Export requests in JSON format"
        @click="${this.onExportClick_}">
      Export
      <cr-icon icon="cr:file-download" slot="suffix-icon"></cr-icon>
    </cr-button>
    <cr-button title="Import requests in JSON format"
        @click="${this.onImportClick_}">
      Import
      <cr-icon icon="suggest:file-upload" slot="suffix-icon"></cr-icon>
    </cr-button>
    <input id="fileInput" type="file" accept=".json" style="display: none;"
      @change="${this.onImportFile_}">
    <cr-button title="Clear the result list" @click="${this.onClearClick_}">
      Clear
      <cr-icon icon="cr:delete" slot="suffix-icon"></cr-icon>
    </cr-button>
  </div>
</cr-drawer>
<div id="requests">
  ${this.hardcodedRequest_ ? html`
    <suggest-request request="${this.hardcodedRequest_}"
        @show-toast="${this.onShowToast_}"
        @open-hardcode-response-dialog="${this.onOpenHardcodeResponseDialog_}"
        @chip-click="${this.populateSearchInput_}">
    </suggest-request>
  ` : ''}
  ${this.requests_.filter(
        request => this.requestFilter_(request)).map(request => html`
    <suggest-request .request="${request}"
        @show-toast="${this.onShowToast_}"
        @open-hardcode-response-dialog="${this.onOpenHardcodeResponseDialog_}"
        @chip-click="${this.populateSearchInput_}">
    </suggest-request>
  `)}
</div>
<cr-dialog id="hardcodeResponseDialog">
  <div slot="header">
    Confirm to hardcode the following response for all Suggest requests.
  </div>
  <div slot="body">
    <cr-input type="text" label="Delay" value="${this.responseDelay_}"
        @value-changed="${this.onResponseDelayChanged_}"
        placeholder="optional delay in milliseconds"
        pattern="[0-9]+"
        error-message="must be a positive integer"
        auto-validate>
    </cr-input>
    <cr-textarea label="Response" value="${this.responseText_}"
        @value-changed="${this.onResponseTextChanged_}" autogrow>
    </cr-textarea>
  </div>
  <div slot="button-container">
    <cr-button class="cancel-button" @click="${this.onCloseDialogs_}">
      Cancel
    </cr-button>
    <cr-button class="action-button"
        @click="${this.onConfirmHardcodeResponseDialog_}">
      Confirm
    </cr-button>
  </div>
</cr-dialog>
<cr-toast id="toast" duration="${this.toastDuration_}">
  <div>${this.toastMessage_}</div>
</cr-toast>
<!--_html_template_end_-->`;
  // clang-format on
}
