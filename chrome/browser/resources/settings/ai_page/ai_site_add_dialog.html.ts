// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {AiSiteAddDialogElement} from './ai_site_add_dialog.js';

export function getHtml(this: AiSiteAddDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" close-text="$i18n{close}" show-on-attach>
  <div slot="title">Add a site</div>
  <div slot="body">
    <cr-input id="site" label="Site"
        placeholder="example.com"
        .value="${this.site}"
        @value-changed="${this.onSiteValueChanged_}"
        ?invalid="${!!this.errorMessage_}"
        error-message="${this.errorMessage_}"
        spellcheck="false"
        autofocus></cr-input>
  </div>
  <div slot="button-container">
    <cr-button class="cancel-button" @click="${this.onCancelClick_}">
      $i18n{cancel}
    </cr-button>
    <cr-button class="action-button" id="add" @click="${this.onSubmitClick_}"
        ?disabled="${this.submitDisabled_}">
      $i18n{add}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
