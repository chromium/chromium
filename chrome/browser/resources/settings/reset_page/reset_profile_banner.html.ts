// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsResetProfileBannerElement} from './reset_profile_banner.js';

export function getHtml(this: SettingsResetProfileBannerElement) {
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" close-text="$i18n{close}" ignore-popstate
    @cancel="${this.onCancel_}">
  <div slot="title">$i18n{resetAutomatedDialogTitle}</div>
  <div slot="body">
    $i18n{resetAutomatedDialogBody}
    <div id="tamperedPrefsList" ?hidden="${!this.showTamperedPrefsList}">
      <ul id="prefs">
        ${this.tamperedPrefs.map(item => html`
          <li>${item}</li>
        `)}
      </ul>
    </div>
  </div>
  <div slot="button-container">
    <cr-button id="learnMore"
        aria-label="$i18n{resetLearnMoreAccessibilityText}"
        @click="${this.onLearnMoreClick_}">
      $i18n{learnMore}
    </cr-button>
    <cr-button class="action-button" id="confirm"
        @click="${this.onConfirmClick_}">
      $i18n{gotIt}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
