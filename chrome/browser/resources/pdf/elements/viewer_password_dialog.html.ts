// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ViewerPasswordDialogElement} from './viewer_password_dialog.js';

export function getHtml(this: ViewerPasswordDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" no-cancel show-on-attach>
  <div slot="title">$i18n{passwordDialogTitle}</div>
  <div slot="body">
    <div id="message">$i18n{passwordPrompt}</div>
    <cr-input id="password" type="password"
        error-message="$i18n{passwordInvalid}" .invalid="${this.invalid}"
        autofocus>
    </cr-input>
  </div>
  <div slot="button-container">
    <cr-button id="submit" class="action-button" @click="${this.submit}">
      $i18n{passwordSubmit}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
