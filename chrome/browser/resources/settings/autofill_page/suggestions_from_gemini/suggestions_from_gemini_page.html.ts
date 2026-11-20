// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsSuggestionsFromGeminiPageElement} from './suggestions_from_gemini_page.js';

export function getHtml(this: SettingsSuggestionsFromGeminiPageElement) {
  return html`<!--_html_template_start_-->
<settings-subpage page-title="$i18n{autofillPersonalContextSettingsTitle}"
    route-path="${this.routePath}" class="multi-card">
  <div class="card">
    <settings-toggle-button id="suggestionsFromGeminiToggle"
        pref-key="generated.find_and_fill_with_gemini"
        label="$i18n{autofillPersonalContextSettingsToggleTitle}"
        sub-label="$i18n{autofillPersonalContextSettingsToggleSummary}"
        @settings-boolean-control-change="${
            this.onToggleSettingsBooleanControlChange_}">
    </settings-toggle-button>
    <cr-link-row id="manageConnectedAppsLinkRow"
        label="$i18n{autofillPersonalContextSettingsManageConnectedAppsTitle}"
        sub-label="$i18n{autofillPersonalContextSettingsManageConnectedAppsSummary}"
        @click="${this.onManageConnectedAppsClick_}" external>
    </cr-link-row>
  </div>
</settings-subpage>
<!--_html_template_end_-->`;
}
