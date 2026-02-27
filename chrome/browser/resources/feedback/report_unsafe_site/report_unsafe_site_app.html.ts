// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ReportUnsafeSiteAppElement} from './report_unsafe_site_app.js';

export function getHtml(this: ReportUnsafeSiteAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="header">
  <h2 class="dialog-title">$i18n{reportUnsafeSiteDialogTitle}</h2>
  $i18nRaw{reportUnsafeSiteDialogDescription}
</div>

<div class="main-content">
  <label class="url-input-container">
    $i18n{reportUnsafeSiteDialogUrlLabel}
    <input type="text" .value="${this.pageUrl_}" readonly/>
  </label>
  <div class="two-cols">
    <div class="hidden-screenshot-image">
      <cr-icon icon="report_unsafe_site:visibility-off"></cr-icon>
    </div>
    <div>
      <cr-checkbox>
        $i18n{reportUnsafeSiteDialogIncludeScreenshotCheckboxLabel}
      </cr-checkbox>
    </div>
  </div>
  <div class="button-container">
    <cr-button class="cancel-button" @click="${this.onCancelButtonClick_}">
      $i18n{cancel}
    </cr-button>
    <cr-button class="action-button">
      $i18n{reportUnsafeSiteDialogSendButtonLabel}
    </cr-button>
  </div>
</div>
<div class="footer">$i18nRaw{reportUnsafeSiteDialogFooter}</div>
<!--_html_template_end_-->`;
  // clang-format on
}
