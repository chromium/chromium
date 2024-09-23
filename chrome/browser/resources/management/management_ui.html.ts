// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ManagementUiElement} from './management_ui.js';

export function getHtml(this: ManagementUiElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-toolbar page-name="$i18n{toolbarTitle}" role="banner" autofocus
    @search-changed="${this.onSearchChanged_}" clear-label="$i18n{clearSearch}"
    search-prompt="$i18n{searchPrompt}">
</cr-toolbar>
<div id="cr-container-shadow-top"
    class="cr-container-shadow has-shadow"></div>
<main id="mainContent">
  <div class="cr-centered-card-container">
    <div class="card">
      <section ?hidden="${!this.managed_}" class="page-subtitle">
        <cr-icon-button class="icon-arrow-back" id="closeButton"
            @click="${this.onTapBack_}" aria-label="$i18n{backButton}">
        </cr-icon-button>
        <h2 class="cr-title-text">${this.subtitle_}</h2>
      </section>
<if expr="chromeos_ash">
      <section class="eol-section" ?hidden="${!this.eolMessage_}">
        <div class="eol-warning-icon">
          <cr-icon icon="cr20:banner-warning"></cr-icon>
        </div>
        <div class="eol-message">
          <div>${this.eolMessage_}</div>
          <div ?hidden="${this.isEolAdminMessageEmpty_()}">
            <div class="eol-admin-title">
              $i18n{updateRequiredEolAdminMessageTitle}
            </div>
            <div>${this.eolAdminMessage_}</div>
          </div>
        </div>
      </section>
</if>

<if expr="not chromeos_ash">
      <section class="overview-section">
        <div .innerHTML="${this.managementNoticeHtml_}"></div>
      </section>
</if>

<if expr="chromeos_ash">
      <section class="overview-section" ?hidden="${!this.managementOverview_}">
        <div class="overview-container">
          <img .src="${this.customerLogo_}" alt="" aria-hidden="true"
              ?hidden="${!this.customerLogo_}">
          <div>${this.managementOverview_}</div>
        </div>
      </section>
</if>

      ${this.showThreatProtectionInfo_() ? html`
        <section>
          <h2 class="cr-title-text">$i18n{threatProtectionTitle}</h2>
          <div class="subtitle">${this.threatProtectionInfo_!.description}</div>
          <table class="content-indented">
            <tr>
              <th class="protection-name">$i18n{connectorEvent}</th>
              <th class="protection-permissions">
                $i18n{connectorVisibleData}
              </th>
            </tr>
            ${this.threatProtectionInfo_!.info.map(item => html`
              <tr>
                <td class="protection-name">${this.i18n(item.title)}</td>
                <td class="protection-permissions">
                  ${this.i18n(item.permission)}
                </td>
              </tr>
            `)}
          </table>
        </section>
      ` : ''}

<if expr="is_chromeos">
      <div ?hidden="${!this.localTrustRoots_}">
        <section>
          <h2 class="cr-title-text">$i18n{localTrustRoots}</h2>
          <div class="subtitle" id="trust-roots-configuration">
            ${this.localTrustRoots_}</div>
        </section>
      </div>
      <div ?hidden="${!this.filesUploadToCloud_}">
         <section>
           <h2 class="cr-title-text">$i18n{filesCloudUpload}</h2>
           <div class="subtitle" id="files-upload-to-cloud-configuration">
             ${this.filesUploadToCloud_}
           </div>
         </section>
      </div>
      ${this.showDeviceReportingInfo_() ? html`
        <section>
          <h2 class="cr-title-text">$i18n{deviceReporting}</h2>
          <div class="subtitle"
              ?hidden="${!this.showMonitoredNetworkPrivacyDisclosure_}">
            $i18n{proxyServerPrivacyDisclosure}
          </div>
          <div class="subtitle">$i18n{deviceConfiguration}</div>
          <div class="content-indented">
            ${this.deviceReportingInfo_!.map(item => html`
              <div class="report">
                <cr-icon
                    icon="${this.getIconForDeviceReportingType_(item.reportingType)}">
                </cr-icon>
                <div .innerHTML="${this.getDeviceReportingHtmlContent_(item)}">
                </div>
              </div>
            `)}
          </div>
          <div class="subtitle"
              ?hidden="${!this.pluginVmDataCollectionEnabled_}">
            $i18nRaw{pluginVmDataCollection}
          </div>
        </section>
      ` : ''}
</if>
<if expr="not is_chromeos">
      ${this.showBrowserReportingInfo_() ? html`
        <section>
          <h2 class="cr-title-text">$i18n{browserReporting}</h2>
          <div class="subtitle">$i18n{browserReportingExplanation}</div>
          <div>
            ${this.browserReportingInfo_!.map(item => html`
              <div class="report">
                <cr-icon icon="${item.icon}"></cr-icon>
                <ul class="browser">
                  ${item.messageIds.map(messageId => html`
                    <li .innerHTML="${this.i18nAdvanced(messageId)}"></li>
                  `)}
                </ul>
              </div>
            `)}
          </div>
        </section>
      ` : ''}

      ${this.showProfileReportingInfo_() ? html`
        <section>
          <h2 class="cr-title-text">$i18n{browserReporting}</h2>
          <div class="subtitle">$i18n{profileReportingExplanation}</div>
          <div>
            <div class="report">
              <ul class="profile">
                ${this.profileReportingInfo_!.map(item => html`
                  <li .innerHTML="${this.i18nAdvanced(item.messageIds[0]!)}">
                  </li>
                `)}
              </ul>
            </div>
          </div>
        </section>
      ` : ''}
</if>
      ${this.showExtensionReportingInfo_() ? html`
        <section class="extension-reporting">
          <h2 class="cr-title-text">$i18n{extensionReporting}</h2>
          <div class="subtitle">${this.extensionReportingSubtitle_}</div>
          <table class="content-indented">
            <tr>
              <th class="extension-name">$i18n{extensionName}</th>
              <th class="extension-permissions">
                $i18n{extensionPermissions}
              </th>
            </tr>
            ${this.extensions_!.map(item => html`
              <tr>
                <td class="extension-name">
                  <div .title="${item.name}" role="presentation">
                    <img .src="${item.icon}" alt="" aria-hidden="true">
                    <span>${item.name}</span>
                  </div>
                </td>
                <td class="extension-permissions">
                  <ul>
                    ${item.permissions.map(permission => html`
                      <li>${permission}</li>
                    `)}
                  </ul>
                </td>
              </tr>
            `)}
          </table>
        </section>
      ` : ''}

      ${this.showManagedWebsitesInfo_() ? html`
        <section class="managed-webistes">
          <h2 class="cr-title-text">$i18n{managedWebsites}</h2>
          <div class="subtitle">${this.managedWebsitesSubtitle_}</div>
          <div class="content-indented">
            ${this.managedWebsites_!.map(item => html`
              <div class="report">${item}</div>
            `)}
          </div>
        </section>
      ` : ''}

      ${this.showApplicationReportingInfo_() ? html`
        <section class="application-reporting">
          <h2 class="cr-title-text">$i18n{applicationReporting}</h2>
          <div class="subtitle">${this.applicationReportingSubtitle_}</div>
          <table class="content-indented">
            <tr>
              <th class="application-name">$i18n{applicationName}</th>
              <th class="extension-permissions">
                $i18n{applicationPermissions}
              </th>
            </tr>
            ${this.applications_!.map(item => html`
              <tr>
                <td class="application-name">
                  <div .title="${item.name}" role="presentation">
                    <img .src="${item.icon}" alt="" aria-hidden="true">
                    <span>${item.name}</span>
                  </div>
                </td>
                <td class="application-permissions">
                  <ul>
                    ${item.permissions.map(permission => html`
                      <li>${permission}</li>
                    `)}
                  </ul>
                </td>
              </tr>
            `)}
          </table>
        </section>
      ` : ''}
    </div>
  </div>
</main>
<!--_html_template_end_-->`;
  // clang-format on
}
