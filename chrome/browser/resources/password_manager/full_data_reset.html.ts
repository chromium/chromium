// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {FullDataResetElement} from './full_data_reset.js';

export function getHtml(this: FullDataResetElement) {
  return html`<!--_html_template_start_-->
<cr-link-row class="cr-row" non-clickable hide-icon
    label="$i18n{fullResetTitle}" sub-label="$i18n{fullResetRowDescription}"
    aria-show-label aria-show-sublabel>
  <cr-button id="deleteAllButton" class="cr-row-action-button"
      @click="${this.onDeleteAllClick_}"
      aria-description="${this.getAriaDescription_()}">
    $i18n{fullResetDeleteAll}
  </cr-button>
</cr-link-row>

<cr-dialog id="dialog">
  <div slot="title" class="dialog-title" id="confirmationDialogTitle">
    ${this.getConfirmationDialogTitle_()}
  </div>
  <div slot="body">
    <div id="detailsContainer">
      <div class="counter-row">
        <cr-icon icon="${this.getPasswordIcon_()}"></cr-icon>
        <div>
          <h3 class="counter-summary">${this.passwordsCount_}</h3>
          <div class="counter-details">${this.passwordsCountDetails_}</div>
        </div>
      </div>
      <div class="counter-row">
        <cr-icon icon="${this.getPasskeyIcon_()}"></cr-icon>
        <div>
          <h3 class="counter-summary">${this.passkeysCount_}</h3>
          <div class="counter-details">${this.passkeysCountDetails_}</div>
        </div>
      </div>
    </div>
    <div id="warningContainer">
      <cr-icon id="warningIcon" icon="${this.getWarningIcon_()}"></cr-icon>
      <div>$i18n{fullResetConfirmationDescription}</div>
    </div>
  </div>
  <div slot="button-container">
    <cr-button class="cancel-button" @click="${this.onCancelClick_}"
        id="cancelButton" autofocus>
      $i18n{cancel}
    </cr-button>
    <cr-button class="action-button" @click="${this.onConfirmClick_}"
        id="confirmButton">
      $i18n{fullResetConfirm}
    </cr-button>
  </div>
</cr-dialog>

<cr-toast id="successToast" duration="5000">
  <span id="toast-message">$i18n{fullResetSuccessToast}</span>
</cr-toast>
<!--_html_template_end_-->`;
}
