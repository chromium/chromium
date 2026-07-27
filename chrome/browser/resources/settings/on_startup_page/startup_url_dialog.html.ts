// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsStartupUrlDialogElement} from './startup_url_dialog.js';

export function getHtml(this: SettingsStartupUrlDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" close-text="$i18n{close}">
  <div slot="title">${this.dialogTitle_}</div>
  <div slot="body">
    <cr-input id="url" label="$i18n{onStartupSiteUrl}"
        .value="${this.url_}"
        @value-changed="${this.onUrlValueChanged_}"
        @input="${this.onInput_}"
        spellcheck="false"
        maxlength="${this.urlLimit_}"
        ?invalid="${this.hasError_()}"
        autofocus
        error-message="${this.getErrorMessage_()}">
    </cr-input>
  </div>
  <div slot="button-container">
    <cr-button class="cancel-button" @click="${this.onCancelClick_}"
        id="cancel">$i18n{cancel}</cr-button>
    <cr-button id="actionButton" class="action-button"
        @click="${this.onActionButtonClick_}">${this.actionButtonText_}</cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
