// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsPrivacyGuideDialogElement} from './privacy_guide_dialog.js';

export function getHtml(this: SettingsPrivacyGuideDialogElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<dialog id="dialog" @cancel="${this.onDialogCancel_}" @close="${this.onDialogClose_}"
    aria-label="$i18n{privacyGuideLabel}">
  <div class="cr-row first" id="headerLine" slot="title">
    <cr-icon-button class="icon-arrow-back" id="backToSettingsButton"
        @click="${this.onSettingsBackClick_}"
        aria-label="$i18n{privacyGuideBackToSettingsAriaLabel}"
        aria-roledescription=
        "$i18n{privacyGuideBackToSettingsAriaRoleDescription}">
    </cr-icon-button>
    <h1 class="cr-title-text">$i18n{privacyGuideLabel}</h1>
  </div>
  <settings-privacy-guide-page @close="${this.onPrivacyGuidePageClose_}"
      slot="body">
  </settings-privacy-guide-page>
</dialog>
<!--_html_template_end_-->`;
  // clang-format on
}
