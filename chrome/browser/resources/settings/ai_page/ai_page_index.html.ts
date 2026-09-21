// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsAiPageIndexElement} from './ai_page_index.js';

export function getHtml(this: SettingsAiPageIndexElement) {
  return html`<!--_html_template_start_-->
<cr-view-manager id="viewManager" class="cr-centered-card-container"
    ?show-all="${this.shouldShowAll}">
  <settings-ai-info-card slot="view" id="aiInfoCard"></settings-ai-info-card>

  ${this.enableAiModeSearchSetting_ ? html`
    <settings-ai-mode-search-page slot="view" id="aiModeSearch">
    </settings-ai-mode-search-page>
  ` : ''}

  ${this.showGlicSettings_ ? html`
    <settings-glic-page slot="view" id="glic">
    </settings-glic-page>

    <settings-glic-subpage slot="view" id="gemini"
        data-parent-view-id="glic"
        route-path="${this.routes_.GEMINI.path}">
    </settings-glic-subpage>
  ` : ''}

  ${this.showGeicSettings_ ? html`
    <settings-geic-page slot="view" id="geic">
    </settings-geic-page>

    <settings-geic-subpage slot="view" id="geminiEnterprise"
        data-parent-view-id="geic"
        route-path="${this.routes_.GEMINI_ENTERPRISE.path}">
    </settings-geic-subpage>
  ` : ''}

  ${this.shouldShowPermissionsPage_() ? html`
    <settings-glic-login-permissions-page slot="view"
        id="geminiLoginPermissions"
        data-parent-view-id="gemini"
        route-path="${this.routes_.GEMINI_LOGIN.path}">
    </settings-glic-login-permissions-page>
  ` : ''}

  ${this.showAiPageAiFeatureSection_ ? html`
    <settings-ai-page slot="view" id="parent">
    </settings-ai-page>
  ` : ''}

  ${this.showHistorySearchControl_ ? html`
    <settings-history-search-page slot="view" id="historySearch"
        data-parent-view-id="parent"
        route-path="${this.routes_.HISTORY_SEARCH.path}">
    </settings-history-search-page>
  ` : ''}

  ${this.showComposeControl_ ? html`
    <settings-offer-writing-help-page slot="view" id="compose"
        data-parent-view-id="parent"
        route-path="${this.routes_.OFFER_WRITING_HELP.path}">
    </settings-offer-writing-help-page>
  ` : ''}

  ${this.showAiSuggestionsControl_ ? html`
    <settings-ai-suggestions-page slot="view" id="aiSuggestions"
        data-parent-view-id="parent"
        route-path="${this.routes_.AI_SUGGESTIONS.path}">
    </settings-ai-suggestions-page>
  ` : ''}

  ${this.showInlineCueMenuControl_ ? html`
    <settings-inline-cue-menu-page slot="view" id="inlineCueMenu"
        data-parent-view-id="parent"
        route-path="${this.routes_.INLINE_CUE_MENU.path}">
    </settings-inline-cue-menu-page>
  ` : ''}

  ${this.showSkillsSettingPage_ ? html`
    <settings-skills-page slot="view" id="skills"
        data-parent-view-id="parent"
        route-path="${this.routes_.SKILLS.path}">
    </settings-skills-page>
  ` : ''}

  ${this.showDictationControl_ ? html`
    <settings-dictation-page slot="view" id="dictation"
        data-parent-view-id="parent"
        route-path="${this.routes_.DICTATION.path}">
    </settings-dictation-page>
  ` : ''}
</cr-view-manager>
<!--_html_template_end_-->`;
}
