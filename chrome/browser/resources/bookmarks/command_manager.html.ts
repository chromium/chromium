// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {BookmarksCommandManagerElement} from './command_manager.js';

export function getHtml(this: BookmarksCommandManagerElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-lazy-render-lit id="dropdown" .template="${() => html`
  <cr-action-menu @mousedown="${this.onMenuMousedown_}"
      role-description="$i18n{menu}">
    ${this.computeMenuCommands_().map(command => html`
      <button class="dropdown-item"
          data-command="${command}"
          ?hidden="${!this.isCommandVisible_(command, this.menuIds_)}"
          ?disabled="${!this.isCommandEnabled_(command, this.menuIds_)}"
          @click="${this.onCommandClick_}">
        ${this.getCommandLabel_(command)}
      </button>
      <hr ?hidden="${!this.showDividerAfter_(command)}"
          aria-hidden="true">
    `)}
  </cr-action-menu>
`}">
</cr-lazy-render-lit>
${this.showEditDialog_ ? html`
  <bookmarks-edit-dialog></bookmarks-edit-dialog>` : ''}
${this.showOpenDialog_ ? html`
  <cr-dialog>
    <div slot="title">$i18n{openDialogTitle}</div>
    <div slot="body"></div>
    <div slot="button-container">
      <cr-button class="cancel-button" @click="${this.onOpenCancelClick_}">
        $i18n{cancel}
      </cr-button>
      <cr-button class="action-button" @click="${this.onOpenConfirmClick_}">
        $i18n{openDialogConfirm}
      </cr-button>
    </div>
  </cr-dialog>` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
