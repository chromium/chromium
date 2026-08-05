// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ExceptionEditDialogElement} from './exception_edit_dialog.js';

export function getHtml(this: ExceptionEditDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" close-text="$i18n{close}" show-on-attach>
  <div slot="title">$i18n{editSiteTitle}</div>
  <div slot="body">
    <tab-discard-exception-edit-input id="input"
        .ruleToEdit="${this.ruleToEdit}"
        @submit-disabled-changed="${this.onSubmitDisabledChanged_}">
    </tab-discard-exception-edit-input>
  </div>
  <div slot="button-container">
    <cr-button id="cancelButton" class="cancel-button"
        @click="${this.onCancelClick_}">
      $i18n{cancel}
    </cr-button>
    <cr-button id="actionButton" class="action-button"
        @click="${this.onSubmitClick_}" ?disabled="${this.submitDisabled_}"
        aria-label="$i18n{tabDiscardingExceptionsSaveButtonAriaLabel}">
      $i18n{save}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
