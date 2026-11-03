// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsResetPageElement} from './reset_page.js';

export function getHtml(this: SettingsResetPageElement) {
  return html`<!--_html_template_start_-->
${this.settingsRefresh2026Enabled_ ? html`
  <cr-button id="resetProfile" @click="${this.onShowResetProfileDialogClick_}">
    $i18n{resetAllSettings}
  </cr-button>
` : html`
  <settings-section page-title="$i18n{resetPageTitle}"
      class="cr-centered-card-container">
    <cr-link-row id="resetProfile" label="$i18n{resetTrigger}"
        @click="${this.onShowResetProfileDialogClick_}">
    </cr-link-row>
  </settings-section>
`}
<cr-lazy-render-lit id="resetProfileDialog"
    .template="${() => html`
      <settings-reset-profile-dialog
          @close="${this.onResetProfileDialogClose_}">
      </settings-reset-profile-dialog>
    `}">
</cr-lazy-render-lit>
<!--_html_template_end_-->`;
}
