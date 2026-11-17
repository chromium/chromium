// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SharePasswordLoadingDialogElement} from './share_password_loading_dialog.js';

export function getHtml(this: SharePasswordLoadingDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog close-text="$i18n{close}" show-on-attach>
  <share-password-dialog-header slot="title">
    ${this.dialogTitle}
  </share-password-dialog-header>
  <div class="spinner" slot="body"></div>
</cr-dialog>
<!--_html_template_end_-->`;
}
