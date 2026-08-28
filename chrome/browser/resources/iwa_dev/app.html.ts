// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {IwaDevAppElement} from './app.js';

export function getHtml(this: IwaDevAppElement) {
  // clang-format off
  return html`
<h1>Isolated Web App Developer Tool</h1>
<div id="learn-more" class="subtitle">
  Install and test Isolated Web Apps during development.
  <a href="https://developer.chrome.com/docs/iwa/introduction"
      target="_blank" rel="noopener"
      aria-label="Learn more about Isolated Web Apps">Learn more</a>
</div>
${!this.devModeEnabled_ ? html`
  <div id="dev-mode-disabled-message">
    ${this.devToolsRestrictedByAdmin_ ? html`
      <p>Isolated Web App Developer Mode is disabled by your administrator.</p>
    ` : html`
      <p>Isolated Web App Developer Mode is disabled.</p>
      <p>To use this page, please enable the
        <a href="chrome://flags#enable-isolated-web-app-dev-mode" target="_blank">
          Isolated Web App Developer Mode
        </a> flag.
      </p>
    `}
  </div>
` : html`
  <div id="content">
    ${!this.hasFetchedApps_ ? '' : html`
      <div class="header-row">
        <div>
          <h2 class="title">
            Installed Applications (${this.installedApps_.length})
          </h2>
          <div id="installed-apps-subtitle" class="subtitle">
            Only apps installed via developer mode appear here.
          </div>
        </div>
        <cr-button class="action-button" id="installButton"
            @click="${this.onOpenInstallDialogClick_}">
          <cr-icon class="icon-16" icon="cr:add" slot="prefix-icon"></cr-icon>
          Install
        </cr-button>
      </div>
      ${this.installedApps_.length === 0 ? html`
        <div id="iwa-list-message">
          No Isolated Web Apps installed in developer mode.
        </div>
      ` : html`
        <div id="iwa-list" role="list">
        ${this.installedApps_.map(item => html`
          <installed-app-list-item
              .app="${item}"
              .isUpdating="${this.updatingAppIds_.includes(item.appId)}"
              role="listitem"
              @request-update="${this.onRequestUpdate_}"
              @request-update-options="${this.onRequestUpdateOptions_}"
              @request-uninstall="${this.onRequestUninstall_}">
          </installed-app-list-item>
        `)}
        </div>
      `}
      <iwa-dev-install-dialog id="installDialog"
          @request-install-from-dev-proxy="${
            this.onRequestInstallFromDevProxy_}"
          @request-install-from-local-bundle="${
            this.onRequestInstallFromLocalBundle_}"
          @request-parse-update-manifest-from-url="${
            this.onRequestParseUpdateManifestFromUrl_}"
          @request-install-from-update-manifest="${
            this.onRequestInstallFromUpdateManifest_}">
      </iwa-dev-install-dialog>
      ${this.selectedAppForUpdateOptions_ ? html`
        <iwa-dev-update-options-dialog id="updateOptionsDialog"
            .app="${this.selectedAppForUpdateOptions_}"
            .currentPinnedVersion="${
              this.getPinnedVersion_(this.selectedAppForUpdateOptions_.appId)}"
            .currentAllowDowngrades="${
              this.getAllowDowngrades_(
                  this.selectedAppForUpdateOptions_.appId)}"
            @close="${this.onUpdateOptionsDialogClose_}"
            @request-parse-update-manifest-from-url="${
              this.onRequestParseUpdateManifestFromUrl_}"
            @update-options-saved="${this.onUpdateOptionsSaved_}">
        </iwa-dev-update-options-dialog>
      ` : ''}
      <cr-toast id="toast" duration="3000">
        <div>${this.toastMessage_}</div>
      </cr-toast>
    `}
  </div>
`}
`;
}
