// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsAiPageElement} from './ai_page.js';

export function getHtml(this: SettingsAiPageElement) {
  return html`<!--_html_template_start_-->
<!-- TODO(crbug.com/362225975): Remove V2 suffixes. -->
<settings-section page-title="$i18n{aiPageTitle}">
  <div>
    <cr-link-row id="passwordChangeRowV2" class="hr"
        ?hidden="${!this.showPasswordChangeControl_}"
        start-icon="cr20:password-manager"
        label="$i18n{passwordChangeSettingLabel}"
        sub-label="$i18n{passwordChangeSettingSubLabel}"
        @click="${this.onPasswordChangeRowClick_}" external>
    </cr-link-row>
    <cr-link-row id="historySearchRowV2" class="hr"
        ?hidden="${!this.showHistorySearchControl_}"
        start-icon="settings20:search-spark"
        label="$i18n{historySearchSettingLabel}"
        sub-label="${this.getHistorySearchSublabel_()}"
        role-description="$i18n{subpageArrowRoleDescription}"
        @click="${this.onHistorySearchRowClick_}">
    </cr-link-row>
    <cr-link-row id="composeRowV2" class="hr"
        ?hidden="${!this.showComposeControl_}"
        start-icon="settings20:pen-spark"
        label="$i18n{aiComposeLabel}"
        sub-label="$i18n{aiComposeSublabelV2}"
        role-description="$i18n{subpageArrowRoleDescription}"
        @click="${this.onComposeRowClick_}">
    </cr-link-row>
    <cr-link-row id="aiSuggestionsRow" class="hr"
        ?hidden="${!this.showAiSuggestionsControl_}"
        start-icon="settings20:button-auto"
        label="$i18n{aiSuggestionsLabel}"
        sub-label="$i18n{aiSuggestionsSublabel}"
        role-description="$i18n{subpageArrowRoleDescription}"
        @click="${this.onAiSuggestionsRowClick_}">
    </cr-link-row>
    <cr-link-row id="inlineCueMenuRow" class="hr"
        ?hidden="${!this.showInlineCueMenuControl_}"
        start-icon="settings20:text-select-end"
        label="$i18n{siteSettingsInlineCueMenu}"
        sub-label="$i18n{siteSettingsInlineCueMenuDescription}"
        role-description="$i18n{subpageArrowRoleDescription}"
        @click="${this.onInlineCueMenuRowClick_}">
    </cr-link-row>
    <cr-link-row id="skillsRow" class="hr"
        ?hidden="${!this.showSkillsSettingPage_}"
        start-icon="settings20:flash-on-filled"
        label="$i18n{skillsSettingLabel}"
        sub-label="$i18n{skillsSettingSublabel}"
        role-description="$i18n{subpageArrowRoleDescription}"
        @click="${this.onSkillsRowClick_}">
    </cr-link-row>
    <cr-link-row id="dictationRow" class="hr"
        ?hidden="${!this.showDictationControl_}"
        start-icon="privacy:mic"
        label="$i18n{dictationSettingLabel}"
        sub-label="$i18n{dictationSettingSublabel}"
        role-description="$i18n{subpageArrowRoleDescription}"
        @click="${this.onDictationRowClick_}">
    </cr-link-row>
    <cr-link-row id="indigoRow" class="hr"
        ?hidden="${!this.showIndigoControl_}"
        start-icon="${this.getIndigoStartIcon_()}"
        label="$i18n{indigoLabel}"
        sub-label="$i18n{indigoSublabel}"
        @click="${this.onIndigoRowClick_}" external>
    </cr-link-row>
    <cr-link-row id="googleSearchAiModeWorkspaceRow" class="hr"
        ?hidden="${!this.showGoogleSearchAiModeWorkspaceControl_}"
        start-icon="settings20:apps"
        label="$i18n{googleSearchAiModeWorkspaceLabel}"
        sub-label="$i18n{googleSearchAiModeWorkspaceSublabel}"
        @click="${this.onGoogleSearchAiModeWorkspaceRowClick_}" external>
    </cr-link-row>
  </div>
</settings-section>
<if expr="_google_chrome">
  <settings-section page-title="$i18n{onDeviceAiEnabledLabel}"
      show-send-feedback-button
      @send-feedback="${this.onOnDeviceAiSendFeedback_}">
    ${this.showOnDeviceAiSettings_ ? html`
      <settings-toggle-button id="onDeviceAiToggle" class="hr"
          .pref="${this.onDeviceAiPref_}"
          label="$i18n{onDeviceAiEnabledLabel}"
          sub-label-with-link="$i18n{onDeviceAiEnabledSubLabel}"
          @sub-label-link-clicked="${this.onOnDeviceAiSubLabelLinkClicked_}"
          @settings-boolean-control-change="${this.onOnDeviceAiSettingsBooleanControlChange_}">
      </settings-toggle-button>
    ` : ''}
  </settings-section>
</if>
<!--_html_template_end_-->`;
}
