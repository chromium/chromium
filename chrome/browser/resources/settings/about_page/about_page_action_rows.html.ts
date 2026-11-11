// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsAboutPageElement} from './about_page.js';

export function getHtml(this: SettingsAboutPageElement) {
  return html`<!--_html_template_start_-->
<if expr="_google_chrome and is_macosx">
${!this.promoteUpdaterStatus_.hidden ? html`
  <div id="promoteUpdater" class="cr-row ${this.getPromoteUpdaterClass_()}"
      ?actionable="${this.promoteUpdaterStatus_.actionable}"
      @click="${this.onPromoteUpdaterClick_}">
    <div class="flex">
      ${this.promoteUpdaterStatus_.text}
      <a href="https://support.google.com/chrome?p=update_chrome"
          target="_blank" id="updaterLearnMore"
          @click="${this.onLearnMoreClick_}"
          aria-label="$i18nPolymer{aboutLearnMoreUpdating}">
        $i18n{learnMore}
      </a>
    </div>
    <cr-icon-button class="subpage-arrow"
        ?hidden="${!this.promoteUpdaterStatus_.actionable}"
        ?disabled="${this.promoteUpdaterStatus_.disabled}"
        aria-label="${this.promoteUpdaterStatus_.text}">
    </cr-icon-button>
  </div>
` : ''}
</if>
<cr-link-row class="hr" id="help" @click="${this.onHelpClick_}"
    label="$i18n{aboutGetHelpUsingChrome}" external>
</cr-link-row>
<if expr="_google_chrome">
<cr-link-row class="hr" id="reportIssue" @click="${this.onReportIssueClick_}"
    ?hidden="${!this.feedbackAllowedPref_?.value}"
    label="$i18n{aboutReportAnIssue}" external>
</cr-link-row>
${!this.settingsRefresh2026Enabled_ ? html`
  <cr-link-row class="hr" id="privacyPolicy"
      @click="${this.onPrivacyPolicyClick_}" label="$i18n{aboutPrivacyPolicy}"
      external>
  </cr-link-row>
` : ''}
</if>
<cr-link-row class="hr" @click="${this.onManagementPageClick_}"
    start-icon="${this.managedByIcon_}" label="$i18n{managementPage}"
    role-description="$i18n{subpageArrowRoleDescription}"
    ?hidden="${!this.isManaged_}">
</cr-link-row>
<!--_html_template_end_-->`;
}
