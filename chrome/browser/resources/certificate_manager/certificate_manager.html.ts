// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CertificateManagerElement} from './certificate_manager.js';
import {CertificateSource} from './certificate_manager.mojom-webui.js';
import {Page} from './navigation.js';

export function getHtml(this: CertificateManagerElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-toolbar id="toolbar"
    page-name="$i18n{certificateManagerV2Title}"
    .showSearch="${this.showSearch_}" always-show-logo>
</cr-toolbar>
<div id="container" class="cr-scrollable">
  <div id="scrollableShadow" class="cr-scrollable-top-shadow"></div>
  <div id="sidebar" role="navigation">
    <cr-menu-selector id="menu" selectable="a" attr-for-selected="href"
        selected-attribute="selected"
        selected="${this.getSelectedTopLevelHref_()}"
        @iron-activate="${this.onMenuIronActivate_}">
      <a id="localMenuItem" role="menuitem" class="cr-nav-menu-item"
          href="${this.generateHrefForPage_(Page.LOCAL_CERTS)}"
          @click="${this.onMenuItemClick_}">
        <cr-icon icon="cr:computer" class="menu-icon"></cr-icon>
        $i18n{certificateManagerV2LocalCerts}
      </a>
      <a id="clientMenuItem" role="menuitem" class="cr-nav-menu-item"
          href="${this.generateHrefForPage_(Page.CLIENT_CERTS)}"
          @click="${this.onMenuItemClick_}">
        <cr-icon icon="certmanager:card" class="menu-icon"></cr-icon>
        $i18n{certificateManagerV2ClientCerts}
      </a>
      <a id="crsMenuItem" role="menuitem" class="cr-nav-menu-item"
          href="${this.generateHrefForPage_(Page.CRS_CERTS)}"
          @click="${this.onMenuItemClick_}">
        <cr-icon icon="cr:chrome-product" class="menu-icon"></cr-icon>
        $i18n{certificateManagerV2CRSCerts}
      </a>
    </cr-menu-selector>
  </div>

  <cr-page-selector id="main" attr-for-selected="path"
      selected="${this.selectedPage_}">

    <div id="clientCertSection" class="cr-centered-card-container"
        path="${Page.CLIENT_CERTS}">
      <h2 class="page-title">$i18n{certificateManagerV2ClientCerts}</h2>
      $i18n{certificateManagerV2ClientCertsDescription}

      <if expr="is_win or is_macosx or is_linux">
        <certificate-list
            id="provisionedClientCerts"
            cert-source="${CertificateSource.kProvisionedClientCert}"
            header-text="$i18n{certificateManagerV2ClientCertsFromAdmin}"
            hide-export hide-if-empty
            @hash-copied="${this.onHashCopied_}">
        </certificate-list>
      </if>
      <if expr="is_chromeos">
        <certificate-list
            id="extensionsClientCerts"
            cert-source="${CertificateSource.kExtensionsClientCert}"
            header-text="$i18n{certificateManagerV2ClientCertsFromExtension}"
            hide-export hide-if-empty
            @hash-copied="${this.onHashCopied_}">
        </certificate-list>
      </if>

      <h3 class="section-title">$i18n{certificateManagerV2Platform}</h3>

      <div class="card">
        <cr-link-row id="viewOsImportedClientCerts"
            label="$i18n{certificateManagerV2PlatformCertsViewLink}"
            sub-label="${this.numPlatformClientCertsString_}"
            @click="${this.onClientPlatformCertsLinkRowClick_}">
        </cr-link-row>
        <if expr="is_win or is_macosx">
          <cr-link-row external id="manageOsImportedClientCerts" class="hr"
              label="$i18n{certificateManagerV2PlatformCertsManageLink}"
              button-aria-description="$i18n{certificateManagerV2PlatformCertsManageLinkAriaDescription}"
              @click="${this.onManageCertsExternalClick_}">
          </cr-link-row>
        </if>
      </div>

      <if expr="is_chromeos">
        <certificate-provisioning-list remove-header-padding>
        </certificate-provisioning-list>
      </if>
    </div>

    <local-certs-section id="localCertSection"
        path="${Page.LOCAL_CERTS}" @hash-copied="${this.onHashCopied_}">
    </local-certs-section>

    <!-- Chrome Root Store section -->
    <crs-section id="crsCertSection"
        path="${Page.CRS_CERTS}" @hash-copied="${this.onHashCopied_}">
    </crs-section>

    <certificate-subpage id="adminCertsSection"
        path="${Page.ADMIN_CERTS}"
        class="cr-centered-card-container"
        @hash-copied="${this.onHashCopied_}"
        navigate-back-target="${Page.LOCAL_CERTS}"
        subpage-title="$i18n{certificateManagerV2AdminCertsTitle}"
        .subpageCertLists="${this.enterpriseSubpageLists_}">
    </certificate-subpage>

    <if expr="not is_chromeos">
      <certificate-subpage id="platformCertsSection"
          path="${Page.PLATFORM_CERTS}"
          class="cr-centered-card-container"
          @hash-copied="${this.onHashCopied_}"
          navigate-back-target="${Page.LOCAL_CERTS}"
          subpage-title="$i18n{certificateManagerV2PlatformCertsTitle}"
          .subpageCertLists="${this.platformSubpageLists_}">
      </certificate-subpage>
    </if>

    <certificate-subpage id="platformClientCertsSection"
        path="${Page.PLATFORM_CLIENT_CERTS}"
        class="cr-centered-card-container"
        @hash-copied="${this.onHashCopied_}"
        navigate-back-target="${Page.CLIENT_CERTS}"
        @import-result="${this.onImportResult_}"
        @delete-result="${this.onDeleteResult_}"
        subpage-title="$i18n{certificateManagerV2ClientCertsFromPlatform}"
        .subpageCertLists="${this.clientPlatformSubpageLists_}">
    </certificate-subpage>

    <certificate-subpage id="userCertsSection"
        path="${Page.USER_CERTS}"
        class="cr-centered-card-container"
        @hash-copied="${this.onHashCopied_}"
        @import-result="${this.onImportResult_}"
        @delete-result="${this.onDeleteResult_}"
        navigate-back-target="${Page.LOCAL_CERTS}"
        subpage-title="$i18n{certificateManagerV2UserCertsTitle}"
        .subpageCertLists="${this.userSubpageLists_}">
    </certificate-subpage>
  </cr-page-selector>
</div>

<cr-toast id="toast" duration="5000">
  <span id="toast-message">${this.toastMessage_}</span>
</cr-toast>

${this.showInfoDialog_ ? html`
  <certificate-info-dialog id="infoDialog"
      .dialogTitle="${this.infoDialogTitle_}"
      .dialogMessage="${this.infoDialogMessage_}"
      @close="${this.onInfoDialogClose_}">
  </certificate-info-dialog>
` : ''}
${this.showPasswordDialog_ ? html`
  <certificate-password-dialog id="passwordDialog"
      @close="${this.onPasswordDialogClose_}">
  </certificate-password-dialog>
` : ''}
${this.showConfirmationDialog_ ? html`
  <certificate-confirmation-dialog id="confirmationDialog"
      .dialogTitle="${this.confirmationDialogTitle_}"
      .dialogMessage="${this.confirmationDialogMessage_}"
      @close="${this.onConfirmationDialogClose_}">
  </certificate-confirmation-dialog>
` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
