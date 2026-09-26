// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsVirtualCardUnenrollDialogElement} from './virtual_card_unenroll_dialog.js';

export function getHtml(this: SettingsVirtualCardUnenrollDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" show-on-attach close-text="$i18n{close}">
  <div slot="title">$i18n{unenrollVirtualCardDialogTitle}</div>
  <div slot="body">
    <div class="cr-padded-text">$i18nRaw{unenrollVirtualCardDialogLabel}</div>
  </div>
  <div slot="button-container">
    <cr-button id="cancelButton" class="cancel-button"
        @click="${this.onCancelButtonClick_}">$i18n{cancel}</cr-button>
    <cr-button id="confirmButton" class="action-button"
        @click="${this.onConfirmButtonClick_}">
      $i18n{unenrollVirtualCardDialogConfirm}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
