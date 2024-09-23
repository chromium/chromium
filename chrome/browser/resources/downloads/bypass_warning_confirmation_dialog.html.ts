// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {DownloadsBypassWarningConfirmationDialogElement} from './bypass_warning_confirmation_dialog.js';

export function getHtml(this: DownloadsBypassWarningConfirmationDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog show-on-attach id="dialog">
  <div slot="title">$i18n{warningBypassDialogTitle}</div>
  <div slot="body" id="body">
    <div id="icon-wrapper" role="img"
        aria-label="$i18n{accessibleLabelDangerous}">
      <cr-icon icon="downloads:dangerous"></cr-icon>
    </div>
    <div id="body-text">
      <div id="file-name">${this.fileName}</div>
      <div id="danger-description">$i18n{warningBypassPromptDescription}</div>
      <div id="learn-more-link">
        <!-- noopener cuts off the script connection between chrome://downloads,
          which has sensitive capabilities, and the newly opened web renderer,
          which may be more readily compromised. -->
        <a href="$i18n{blockedLearnMoreUrl}" target="_blank" rel="noopener">
          $i18n{warningBypassPromptLearnMoreLink}
        </a>
      </div>
    </div>
  </div>
  <div slot="button-container">
    <cr-button class="tonal-button" @click="${this.onDownloadDangerousClick_}"
        id="download-dangerous-button">
      $i18n{controlKeepDangerous}
    </cr-button>
    <!-- The cancel button is the primary action because we don't want the user
      to download the dangerous file. -->
    <cr-button class="action-button" @click="${this.onCancelClick_}"
        id="cancel-button">
      $i18n{warningBypassDialogCancel}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
