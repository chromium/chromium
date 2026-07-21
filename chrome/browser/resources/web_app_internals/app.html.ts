// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {WebAppInternalsAppElement} from './app.js';

export function getHtml(this: WebAppInternalsAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<button id="download-button" @click="${this.onDownloadButtonClick_}">
  Download
</button>
<button id="copy-button" @click="${this.onCopyButtonClick_}">
  Copy to Clipboard
</button>

<hr>

<div id="iwa-container"
    ?hidden="${!this.isIwaPolicyInstallEnabled_ && !this.isIwaDevModeEnabled_}">
  <h2>Isolated Web Apps</h2>

  <div id="iwa-updates-container" ?hidden="${!this.isIwaPolicyInstallEnabled_}">
    <button id="iwa-updates-search-button"
        @click="${this.onIwaUpdatesSearchButtonClick_}">
      Discover updates of policy-installed IWAs now
    </button>
    <div id="iwa-updates-message">${this.iwaUpdatesMessage_}</div>
  </div>

  <div id="iwa-dev-container" ?hidden="${!this.isIwaDevModeEnabled_}">
    <h3>Developer Mode</h3>
    <p>
      Install IWA via Dev Mode Proxy:
      <input type="url" id="devInstallProxyUrl" size="30" required
          aria-label="Install IWA via Dev Mode Proxy URL"
          placeholder="http://localhost:8000/"
          .value="${this.iwaDevInstallProxyUrl_}"
          @input="${this.onIwaDevInstallProxyUrlInput_}"
          @keyup="${this.onIwaDevInstallProxyUrlKeyup_}">
      <button id="iwa-dev-install-proxy-button" type="submit"
          ?disabled="${this.isInstallingProxy_ || !this.iwaDevInstallProxyUrl_}"
          @click="${this.onIwaDevInstallProxyButtonClick_}">
        Install
      </button>
    </p>
    <p>
      Install IWA from Signed Web Bundle:
      <button id="iwa-dev-install-bundle-selector" type="submit"
          @click="${this.onIwaDevInstallBundleSelectorClick_}">
        Select file...
      </button>
    </p>
    <p>
      Install IWA from Update Manifest:
      <input type="url" id="devUpdateManifestUrl" size="50" required
          aria-label="Install IWA from Update Manifest URL"
          placeholder="http://localhost:8000/update_manifest.json"
          .value="${this.iwaDevUpdateManifestUrl_}"
          @input="${this.onIwaDevUpdateManifestUrlInput_}"
          @keyup="${this.onIwaDevUpdateManifestUrlKeyup_}">
      <button id="iwa-dev-update-manifest-fetch-button" type="submit"
          @click="${this.onIwaDevUpdateManifestFetchButtonClick_}">
        Fetch
      </button>
    </p>

    <dialog id="updateManifestDialog">
      <div>Installing IWA</div>
      <div id="iwa-update-manifest-version">
        <div id="iwa-update-manifest-version-title">Select Version:</div>
        <select id="updateManifestVersionSelect" aria-label="Select Version">
          ${this.manifestVersions_.map(item => html`
            <option .value="${item.version}">${item.version}</option>
          `)}
        </select>
      </div>
      <div id="iwa-update-manifest-dialog-buttons">
        <button id="iwa-update-manifest-dialog-close"
            @click="${this.onIwaUpdateManifestCloseClick_}">Close</button>
        <button id="iwa-update-manifest-dialog-install"
            @click="${this.onIwaUpdateManifestInstallClick_}">Install</button>
      </div>
    </dialog>

    <dialog id="switchChannelInputDialog" class="update-input-dialog">
      <div>Switching update channel for IWA</div>
      <div>New Channel:</div>
      <input type="text" id="updateChannel" size="15" required
          aria-label="New Channel" placeholder="default">
      <div class="dialog-buttons">
        <button id="iwa-switch-channel-dialog-close"
            @click="${this.onIwaSwitchChannelCloseClick_}">Close</button>
        <button id="iwa-switch-channel-dialog-switch"
            @click="${this.onIwaSwitchChannelSwitchClick_}">Switch</button>
      </div>
    </dialog>

    <dialog id="pinnedVersionInputDialog" class="update-input-dialog">
      <div>Pinned Version:</div>
      <input type="text" id="pinnedVersion" size="15" aria-label="Pinned Version">
      <div class="dialog-buttons">
        <button id="iwa-pinned-version-dialog-close"
            @click="${this.onIwaPinnedVersionCloseClick_}">Close</button>
        <button id="iwa-pinned-version-dialog-unpin"
            @click="${this.onIwaPinnedVersionUnpinClick_}">Unpin</button>
        <button id="iwa-pinned-version-dialog-pin"
            @click="${this.onIwaPinnedVersionPinClick_}">Pin</button>
      </div>
    </dialog>

    <div id="iwa-dev-install-message">${this.iwaDevInstallMessage_}</div>
    <h4>Installed Dev Mode IWAs</h4>
    <ul id="iwa-dev-updates-app-list">
      ${this.devModeApps_.map(item => html`
        <li class="iwa-dev-mode-list-item">
          ${this.describeIsolatedWebApp_(item)}
          <div class="dev-iwa-buttons">
            <button data-app-id="${item.appId}"
                ?disabled="${!!item.isUpdating}"
                @click="${this.onUpdateAppClick_}">
              ${this.getUpdateButtonLabel_(item)}
            </button>
            <button data-app-id="${item.appId}"
                ?disabled="${!!item.isDeleting}"
                @click="${this.onDeleteAppClick_}">
              ${item.isDeleting ? 'Deleting IWA...' : 'Delete IWA'}
            </button>
            ${item.updateInfo ? html`
              <button data-app-id="${item.appId}" data-app-name="${item.name}"
                  @click="${this.onSwitchChannelClick_}">
                Switch channel
              </button>
              <button data-app-id="${item.appId}" data-app-name="${item.name}"
                  @click="${this.onPinToVersionClick_}">
                Pin To Version
              </button>
              <input type="checkbox"
                  id="allow-downgrades-toggle-${item.appId}"
                  data-app-id="${item.appId}"
                  .checked="${item.updateInfo.allowDowngrades || false}"
                  @change="${this.onAllowDowngradesChange_}">
              <label for="allow-downgrades-toggle-${item.appId}">Allow downgrades</label>
            ` : ''}
          </div>
          ${item.updateMsg ? html`<p>${item.updateMsg}</p>` : ''}
        </li>
      `)}
    </ul>
    <div id="iwa-dev-updates-message">${this.devModeUpdatesMessage_}</div>
  </div>
  <hr>
</div>

<div id="app-index" role="navigation" aria-label="Installed web apps">
  ${this.getAppIndexEntries_().map(item => html`
    <a href="${item.id ? '#' + item.id : '#'}"
        class="${item.isActive ? 'active' : ''}">
      ${item.label}
    </a>
  `)}
</div>
<pre id="json">${this.getFormattedJson_()}</pre>
<!--_html_template_end_-->`;
  // clang-format on
}
