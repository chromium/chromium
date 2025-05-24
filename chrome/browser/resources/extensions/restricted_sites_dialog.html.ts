// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ExtensionsRestrictedSitesDialogElement} from './restricted_sites_dialog.js';

export function getHtml(this: ExtensionsRestrictedSitesDialogElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" show-on-attach>
  <div slot="title">${this.getDialogTitle_()}</div>
  <div class="matching-restricted-sites-warning" slot="body">
    <cr-icon icon="cr:info-outline"></cr-icon>
    <span>${this.getDialogWarning_()}</span>
  </div>
  <div slot="button-container">
    <cr-button class="cancel-button" @click="${this.onCancelClick_}">
      $i18n{cancel}
    </cr-button>
    <cr-button class="action-button" @click="${this.onSubmitClick_}">
      $i18n{matchingRestrictedSitesAllow}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
  // clang-format on
}
