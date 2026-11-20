// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {routes} from '../route.js';

import type {SettingsPrivacyPageElement} from './privacy_page.js';

export function getHtml(this: SettingsPrivacyPageElement) {
  return html`<!--_html_template_start_-->
<settings-section page-title="$i18n{privacyPageTitle}">
  ${this.showClearBrowsingDataDialog_ ? html`
    <settings-clear-browsing-data-dialog @close="${this.onCbdDialogClose_}"
        @browsing-data-deleted="${this.onBrowsingDataDeleted_}">
    </settings-clear-browsing-data-dialog>
  ` : ''}
  ${this.showPrivacyGuideDialog_ ? html`
    <settings-privacy-guide-dialog id="privacyGuideDialog"
        @close="${this.onPrivacyGuideDialogClose_}">
    </settings-privacy-guide-dialog>
  ` : ''}
  <cr-link-row id="clearBrowsingData"
      start-icon="${routes.CLEAR_BROWSER_DATA.getIcon()}"
      label="$i18n{clearBrowsingData}"
      sub-label="$i18n{clearBrowsingDataDescription}"
      @click="${this.onClearBrowsingDataClick_}">
  </cr-link-row>
  ${this.isPrivacyGuideAvailable ? html`
    <cr-link-row id="privacyGuideLinkRow" class="hr"
        start-icon="${routes.PRIVACY_GUIDE.getIcon()}"
        label="$i18n{privacyGuideLabel}" sub-label="$i18n{privacyGuideSublabel}"
        @click="${this.onPrivacyGuideClick_}"
        role-description="$i18n{subpageArrowRoleDescription}">
    </cr-link-row>
  ` : ''}
  <cr-link-row id="thirdPartyCookiesLinkRow"
      start-icon="${routes.COOKIES.getIcon()}" class="hr"
      label="${this.getThirdPartyCookiesLabel_()}"
      sub-label="${this.getThirdPartyCookiesSublabel_()}"
      @click="${this.onCookiesClick_}"
      role-description="$i18n{subpageArrowRoleDescription}">
  </cr-link-row>
  <cr-link-row id="securityLinkRow" start-icon="${routes.SECURITY.getIcon()}"
      class="hr" label="$i18n{securityPageTitle}"
      sub-label="$i18n{securityPageDescription}"
      @click="${this.onSecurityPageClick_}"
      role-description="$i18n{subpageArrowRoleDescription}">
  </cr-link-row>
  <cr-link-row id="siteSettingsLinkRow"
      start-icon="${routes.SITE_SETTINGS.getIcon()}" class="hr"
      label="$i18n{siteSettings}" sub-label="$i18n{siteSettingsSublabel}"
      @click="${this.onSiteSettingsLinkRowClick_}"
      role-description="$i18n{subpageArrowRoleDescription}">
  </cr-link-row>
  <cr-toast id="deleteBrowsingDataToast" duration="5000">
    <div>${this.dbdDeletionConfirmationToastLabel_}</div>
  </cr-toast>
</settings-section>
<!--_html_template_end_-->`;
}
