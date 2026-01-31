// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {IssueDetailsElement} from './issue_details.js';

export function getHtml(this: IssueDetailsElement) {
  // clang-format off
  return html`<!--html_template_start_-->
    <h1 tabindex="0">${this.i18n('issueDetailsPageTitle')}</h1>
    <div class="support-tool-title">${this.i18n('supportCaseId')}</div>
    <cr-input class="support-case-id" .value="${this.caseId_}"
        spellcheck="false" maxlength="20"
        aria-label="${this.i18n('supportCaseId')}">
    </cr-input>
    <div id="email-title" class="support-tool-title" aria-hidden="true">
      ${this.i18n('email')}
    </div>
    <select class="md-select" .value="${this.selectedEmail_}"
        aria-labelledby="email-title"
        @change="${this.onSelectedEmailChange_}">
      ${this.emails_.map((item: string) => html`
        <option .value="${item}">${item}</option>
      `)}
    </select>
    <div id="description-title" class="support-tool-title" aria-hidden="true">
      ${this.i18n('describeIssueText')}
    </div>
    <cr-textarea id="description" spellcheck="true"
        .value="${this.issueDescription_}"
        aria-labelledby="description-title"
        placeholder="${this.i18n('issueDescriptionPlaceholder')}"
        @input="${this.onIssueDescriptionInput_}">
    </cr-textarea>
<!--html_template_end_-->`;
  // clang-format on
}
