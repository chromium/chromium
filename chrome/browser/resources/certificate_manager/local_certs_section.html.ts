// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {LocalCertsSectionElement} from './local_certs_section.js';

export function getHtml(this: LocalCertsSectionElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="cr-centered-card-container">
  <h2 class="page-title">$i18n{certificateManagerV2LocalCerts}</h2>
  $i18n{certificateManagerV2LocalCertsDescription}

  <if expr="not is_chromeos">
    <h3 class="section-title">$i18n{certificateManagerV2Platform}</h3>
    <div class="card">
      <div class="cr-row">
        <div class="cr-padded-text">
          <div>
            $i18n{certificateManagerV2PlatformCertsToggleLabel}
          </div>
          <div id="numSystemCerts" class="cr-secondary-text">
            ${this.numSystemCertsString_}
          </div>
        </div>
        <cr-toggle id="importOsCerts"
            aria-label="$i18n{certificateManagerV2PlatformCertsToggleLabel}"
            ?checked="${this.importOsCertsEnabled_()}"
            ?disabled="${this.importOsCertsEnabledManaged_()}"
            @change="${this.onOsCertsToggleChange_}">
        </cr-toggle>
        <cr-icon id="importOsCertsManagedIcon" icon="cr:domain"
            class="enterprise-icon"
            ?hidden="${!this.importOsCertsEnabledManaged_()}">
        </cr-icon>
      </div>
      <cr-link-row id="viewOsImportedCerts" class="hr"
          label="$i18n{certificateManagerV2PlatformCertsViewLink}"
          @click="${this.onPlatformCertsLinkRowClick_}"
          ?hidden="${!this.showViewOsCertsLinkRow_()}">
      </cr-link-row>
      <if expr="is_win or is_macosx">
        <cr-link-row external id="manageOsImportedCerts" class="hr"
            ?hidden="${!this.importOsCertsEnabled_()}"
            label="$i18n{certificateManagerV2PlatformCertsManageLink}"
            @click="${this.onManageCertsExternalClick_}"
            button-aria-description=
                "$i18n{certificateManagerV2PlatformCertsManageLinkAriaDescription}">
        </cr-link-row>
      </if>
    </div>
  </if>

  ${this.showCustomSection_() ? html`
    <div id="customCertsSection">
      <h3 class="section-title">$i18n{certificateManagerV2CustomCertsTitle}</h3>
      <div class="card">
        ${this.showPolicySection_() ? html`
          <cr-link-row id="adminCertsInstalledLinkRow"
              start-icon="cr:domain"
              label="$i18n{certificateManagerV2AdminCertsTitle}"
              sub-label="${this.numPolicyCertsString_}"
              @click="${this.onAdminCertsInstalledLinkRowClick_}">
          </cr-link-row>
        ` : ''}
        ${this.showUserSection_() ? html`
          <div id="userCertsSection">
            <cr-link-row id="userCertsInstalledLinkRow"
                start-icon="cr:person"
                label="$i18n{certificateManagerV2UserCertsTitle}"
                sub-label="${this.numUserCertsString_}"
                @click="${this.onUserCertsInstalledLinkRowClick_}">
            </cr-link-row>
          </div>
        ` : ''}
      </div>
    </div>
  ` : ''}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
