// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsHistoryDeletionDialogElement} from './history_deletion_dialog.js';

export function getHtml(this: SettingsHistoryDeletionDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" close-text="$i18n{close}" show-on-attach>
  <div slot="title">$i18n{historyDeletionDialogTitle}</div>
  <div slot="body">$i18nRaw{historyDeletionDialogBody}</div>
  <div slot="button-container">
    <cr-button id="okButton" class="action-button" @click="${this.onOkClick_}">
      $i18n{historyDeletionDialogOK}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
