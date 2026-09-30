// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsAddressRemoveConfirmationDialogElement} from './address_remove_confirmation_dialog.js';

export function getHtml(this: SettingsAddressRemoveConfirmationDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog show-on-attach id="dialog" close-text="$i18n{close}">
  <div slot="title" id="title">${this.getConfirmationTitle_()}</div>
  <div slot="body" id="description"
      .innerHTML="${this.getConfirmationDescription_()}">
  </div>
  <div slot="button-container">
    <cr-button class="cancel-button" @click="${this.onCancelClick}" id="cancel">
      $i18n{cancel}
    </cr-button>
    <cr-button class="action-button" @click="${this.onRemoveClick}" id="remove">
      ${this.getRemoveButtonLabel_()}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
