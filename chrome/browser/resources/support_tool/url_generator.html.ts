// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {DataCollectorItem} from './browser_proxy.js';
import type {UrlGeneratorElement} from './url_generator.js';

export function getHtml(this: UrlGeneratorElement) {
  // clang-format off
  return html`<!--html_template_start_-->
<h1 tabindex="0">${this.i18n('urlGeneratorPageTitle')}</h1>
<div class="support-tool-title">${this.i18n('supportCaseId')}</div>
<cr-input id="caseIdInput" class="support-case-id" .value="${this.caseId_}"
    @input="${this.onCaseIdInput_}" spellcheck="false" maxlength="20"
    aria-label="${this.i18n('supportCaseId')}">
</cr-input>
<div id="data-sources-title" class="support-tool-title" tabindex="0">
  ${this.i18n('dataCollectorListTitle')}
</div>

<cr-checkbox class="select-all-checkbox" id="selectAllCheckbox"
    ?checked="${this.selectAll_}"
    @checked-changed="${this.onSelectAllCheckboxCheckedChanged_}" tabindex="0">
  ${this.i18n('selectAll')}
</cr-checkbox>

<div class="data-collector-list" role="group"
    aria-labelledby="data-sources-title">
  ${this.dataCollectors_.map(
      (item: DataCollectorItem, index: number) => html`
    <cr-checkbox class="data-collector-checkbox"
        ?checked="${item.isIncluded}" data-index="${index}"
        @checked-changed="${this.onDataCollectorCheckedChanged_}" tabindex="0">
      ${item.name}
    </cr-checkbox>
  `)}
</div>

<div class="support-tool-title" tabindex="0">${this.i18n('getLinkText')}</div>
<div>
  <p id="info-text">
    ${this.i18n('copyLinkDescription')}
  </p>
  <cr-button id="copyURLButton" class="navigation-buttons action-button"
      @click="${this.onCopyUrlClick_}" ?disabled="${this.buttonDisabled_}">
      ${this.i18n('copyLinkButtonText')}
  </cr-button>
  <cr-button id="copyTokenButton" class="navigation-buttons action-button"
      @click="${this.onCopyTokenClick_}" ?disabled="${this.buttonDisabled_}">
      ${this.i18n('copyTokenButtonText')}
  </cr-button>
</div>
<cr-toast id="copyToast" duration="5000" tabindex="0"
    aria-labelledby="link-copied-message">
  <span id="link-copied-message">${this.copiedToastMessage_}</span>
</cr-toast>
<cr-toast id="errorMessageToast" duration="0" tabindex="0"
    aria-labelledby="error-message">
  <span id="error-message">${this.errorMessage_}</span>
  <cr-button @click="${this.onErrorMessageToastCloseClick_}">
    ${this.i18n('dismissButtonText')}
  </cr-button>
</cr-toast>
  <!--html_template_end_-->`;
  // clang-format on
}
