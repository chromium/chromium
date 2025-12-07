// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ExtensionsPackDialogAlertElement} from './pack_dialog_alert.js';

export function getHtml(this: ExtensionsPackDialogAlertElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" close-text="$i18n{close}" show-on-attach>
  <div class="title" slot="title">${this.title_}</div>
  <!-- No whitespace or new-lines allowed within the div.body tag. -->
  <div class="body" slot="body">${this.model.message}</div>
  <div class="button-container" slot="button-container">
    <cr-button class="${this.getCancelButtonClass_()}"
        @click="${this.onCancelClick_}" ?hidden="${!this.cancelLabel_}">
      ${this.cancelLabel_}
    </cr-button>
    <cr-button class="action-button" @click="${this.onConfirmClick_}"
        ?hidden="${!this.confirmLabel_}">
      ${this.confirmLabel_}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
  // clang-format on
}
