// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ExtensionsRuntimeHostsDialogElement} from './runtime_hosts_dialog.js';

export function getHtml(this: ExtensionsRuntimeHostsDialogElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" close-text="$i18n{close}">
  <div slot="title">${this.computeDialogTitle_()}</div>
  <div slot="body">
    <cr-input id="input" label="$i18n{runtimeHostsDialogInputLabel}"
        placeholder="http://example.com" .value="${this.site_}"
        @value-changed="${this.onSiteChanged_}" @input="${this.validate_}"
        ?invalid="${this.inputInvalid_}"
        error-message="$i18n{runtimeHostsDialogInputError}" spellcheck="false"
        autofocus>
    </cr-input>
    <div class="matching-restricted-sites-warning"
        ?hidden="${!this.matchingRestrictedSites_.length}">
      <cr-icon icon="cr:info-outline"></cr-icon>
      <span>${this.computeMatchingRestrictedSitesWarning_()}</span>
    </div>
  </div>
  <div slot="button-container">
    <cr-button class="cancel-button" @click="${this.onCancelClick_}">
      $i18n{cancel}
    </cr-button>
    <cr-button class="action-button" id="submit" @click="${this.onSubmitClick_}"
        ?disabled="${this.computeSubmitButtonDisabled_()}">
      ${this.computeSubmitButtonLabel_()}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
  // clang-format on
}
