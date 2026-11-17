// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SharePasswordErrorDialogElement} from './share_password_error_dialog.js';

export function getHtml(this: SharePasswordErrorDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" close-text="$i18n{close}" show-on-attach>
  <div slot="title">
    <share-password-dialog-header id="header" is-error>
      $i18n{sharePasswordErrorTitle}
    </share-password-dialog-header>
  </div>
  <div slot="body" class="flex-centered" id="description">
    $i18n{sharePasswordErrorDescription}
  </div>
  <div slot="button-container">
    <cr-button class="cancel-button" @click="${this.onCancelClick_}"
        id="cancel">
      $i18n{cancel}
    </cr-button>
    <cr-button class="action-button" @click="${this.onTryAgainClick_}"
        id="tryAgain">
      $i18n{sharePasswordTryAgain}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
