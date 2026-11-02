// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsResetProfileDialogElement} from './reset_profile_dialog.js';

export function getHtml(this: SettingsResetProfileDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" close-text="$i18n{close}" ignore-popstate
    ignore-enter-key>
  <div slot="title">
    ${this.getPageTitle_()}
  </div>
  <div id="dialog-body" slot="body">
    <span .innerHTML="${this.getExplanationText_()}"></span>
    <a href="$i18nRaw{resetPageLearnMoreUrl}"
        aria-label="$i18n{resetLearnMoreAccessibilityText}" target="_blank">
      $i18n{learnMore}
    </a>
  </div>
  <div slot="button-container">
    <div class="spinner" ?hidden="${!this.clearingInProgress_}"></div>
    <cr-button class="cancel-button" @click="${this.onCancelClick_}" id="cancel"
        ?disabled="${this.clearingInProgress_}">
      $i18n{cancel}
    </cr-button>
    <cr-button class="action-button" @click="${this.onResetClick_}" id="reset"
        ?disabled="${this.clearingInProgress_}">
      $i18n{resetDialogCommit}
    </cr-button>
  </div>
  <div slot="footer">
    <cr-checkbox id="sendSettings" checked>
      $i18nRaw{resetPageFeedback}
    </cr-checkbox>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
