// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsDefaultBrowserPageElement} from './default_browser_page.js';

export function getHtml(this: SettingsDefaultBrowserPageElement) {
  return html`<!--_html_template_start_-->
<settings-section page-title="$i18n{defaultBrowser}"
    class="cr-centered-card-container">
  ${this.maySetDefaultBrowser_ ? html`
    <div class="cr-row first">
      <div class="flex cr-padded-text">
        <div id="canBeDefaultBrowser">$i18n{defaultBrowser}</div>
        <div class="secondary" id="makeDefaultLabel">
          ${this.getMakeDefaultLabel()}
        </div>
      </div>
      <div class="separator"></div>
      <cr-button @click="${this.onSetDefaultBrowserClick_}">
        $i18n{defaultBrowserMakeDefaultButton}
      </cr-button>
    </div>
  ` : html`
    <div class="cr-row first">
      <div class="flex cr-padded-text" ?hidden="${!this.isDefault_}"
          id="isDefault">
        $i18n{defaultBrowserDefault}
      </div>
      <div class="flex cr-padded-text" ?hidden="${!this.isSecondaryInstall_}"
          id="isSecondaryInstall">
        $i18n{defaultBrowserSecondary}
      </div>
      <div class="cr-padded-text" ?hidden="${!this.isUnknownError_}"
          id="isUnknownError">
        $i18n{defaultBrowserError}
      </div>
    </div>
  `}
</settings-section>
<!--_html_template_end_-->`;
}
