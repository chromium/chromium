// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ViewerErrorDialogElement} from './viewer_error_dialog.js';

export function getHtml(this: ViewerErrorDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" no-cancel show-on-attach>
  <div slot="title">$i18n{errorDialogTitle}</div>
  <div slot="body">$i18n{pageLoadFailed}</div>
  <div slot="button-container" ?hidden="${!this.reloadFn}">
    <cr-button class="action-button" @click="${this.onReload_}">
      $i18n{pageReload}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
