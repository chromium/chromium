// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SupportToolElement} from './support_tool.js';
import {SupportToolPageIndex} from './support_tool.js';

export function getHtml(this: SupportToolElement) {
  // clang-format off
  return html`<!--html_template_start_-->
<div id="support-tool-pages">
  <cr-page-selector .selected="${this.selectedPage_}"
      attr-for-selected="page-index">
    <issue-details id="issueDetails"
        page-index="${SupportToolPageIndex.ISSUE_DETAILS}">
    </issue-details>
    <data-collectors id="dataCollectors"
        page-index="${SupportToolPageIndex.DATA_COLLECTOR_SELECTION}">
    </data-collectors>
    <spinner-page id="spinnerPage"
        .pageTitle="${this.i18n('dataCollectionSpinner')}"
        page-index="${SupportToolPageIndex.SPINNER}">
    </spinner-page>
    <pii-selection id="piiSelection"
        page-index="${SupportToolPageIndex.PII_SELECTION}">
    </pii-selection>
    <spinner-page id="exportSpinner"
        .pageTitle="${this.i18n('dataExportSpinner')}"
        page-index="${SupportToolPageIndex.EXPORT_SPINNER}">
    </spinner-page>
    <data-export-done id="dataExportDone"
        page-index="${SupportToolPageIndex.DATA_EXPORT_DONE}">
    </data-export-done>
  </cr-page-selector>
</div>
<div class="navigation-buttons" id="continueButtonContainer"
    ?hidden="${this.shouldHideContinueButtonContainer_()}">
  <cr-button id="backButton" ?hidden="${this.shouldHideBackButton_()}"
      @click="${this.onBackClick_}">
    ${this.i18n('backButtonText')}
  </cr-button>
  <cr-button id="continueButton" class="action-button"
      @click="${this.onContinueClick_}">
    ${this.i18n('continueButtonText')}
  </cr-button>
</div>
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
