// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {BookmarksEditDialogElement} from './edit_dialog.js';

export function getHtml(this: BookmarksEditDialogElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog">
  <div slot="title">${this.getDialogTitle_()}</div>
  <div slot="body">
    <cr-input id="name" label="$i18n{editDialogNameInput}"
        value="${this.titleValue_}"
        @value-changed="${this.onTitleValueChanged_}" autofocus>
    </cr-input>
    <cr-input id="url" type="url" label="$i18n{editDialogUrlInput}"
        error-message="$i18n{editDialogInvalidUrl}" value="${this.urlValue_}"
        @value-changed="${this.onUrlValueChanged_}"
        ?hidden="${this.isFolder_}" required>
    </cr-input>
  </div>
  <div slot="button-container">
    <cr-button class="cancel-button" @click="${this.onCancelButtonClick_}">
      $i18n{cancel}
    </cr-button>
    <cr-button id="saveButton" class="action-button"
        @click="${this.onSaveButtonClick_}">
      $i18n{saveEdit}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
  // clang-format on
}
