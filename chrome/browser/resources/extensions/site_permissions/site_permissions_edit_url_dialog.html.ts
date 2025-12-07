// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SitePermissionsEditUrlDialogElement} from './site_permissions_edit_url_dialog.js';

export function getHtml(this: SitePermissionsEditUrlDialogElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" show-on-attach>
  <div slot="title">${this.computeDialogTitle_()}</div>
  <div slot="body">
    <cr-input id="input" label="$i18n{sitePermissionsDialogInputLabel}"
        placeholder="https://example.com" .value="${this.site_}"
        @value-changed="${this.onSiteChanged_}" @input="${this.validate_}"
        ?invalid="${!this.inputValid_}"
        error-message="$i18n{sitePermissionsDialogInputError}"
        spellcheck="false" autofocus>
    </cr-input>
  </div>
  <div slot="button-container">
    <cr-button class="cancel-button" @click="${this.onCancel_}">
      $i18n{cancel}
    </cr-button>
    <cr-button class="action-button" id="submit" @click="${this.onSubmit_}"
        ?disabled="${this.computeSubmitButtonDisabled_()}">
      ${this.computeSubmitButtonLabel_()}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
  // clang-format on
}
