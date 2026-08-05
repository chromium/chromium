// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {IwaDevInstallUpdateManifestTabElement} from './install_update_manifest_tab.js';
import {PLACEHOLDER_URL} from './install_update_manifest_tab.js';

export function getHtml(this: IwaDevInstallUpdateManifestTabElement) {
  // clang-format off
  return html`
<div class="fetch-row">
  <cr-input
      type="url"
      label="Update Manifest URL"
      placeholder="${PLACEHOLDER_URL}"
      .value="${this.url_}"
      @value-changed="${this.onUrlValueChanged_}"
      @keydown="${this.onInputKeydown_}"
      ?disabled="${this.isFetching_}"
      ?invalid="${!!this.urlError_}"
      .errorMessage="${this.urlError_}"
      autofocus>
  </cr-input>
  <cr-button
      id="fetchButton"
      @click="${this.onFetchClick_}"
      ?disabled="${this.isFetching_ || !this.url_}">
    Fetch
  </cr-button>
</div>
${this.isManifestFetched_ && !this.urlError_ ? html`
  <div id="fetchSuccessMessage" class="success-message" aria-live="polite">
    Manifest loaded successfully: ${this.versions_.length} version${
        this.versions_.length === 1 ? '' : 's'} available.
  </div>
` : ''}
<div id="details">
  ${!this.isManifestFetched_ ? html`
    <div class="placeholder-message">
      Enter a URL and click Fetch to load manifest details.
    </div>
  ` : html`
    <div id="dropdowns-row">
      <div class="dropdown-container">
        <label for="versionSelect">Version</label>
        <select id="versionSelect"
            class="md-select"
            .value="${this.selectedVersion_}"
            @change="${this.onVersionChange_}"
            @keydown="${this.onSelectKeydown_}"
            ?disabled="${this.disabled}">
          ${this.versions_.map((item, index) => html`
            <option value="${item.version}">
              ${item.version}${index === 0 ? ' (Latest)' : ''}
            </option>
          `)}
        </select>
      </div>
      ${this.channels_.length > 0 ? html`
        <div class="dropdown-container">
          <label for="channelSelect">Update Channel</label>
          <select id="channelSelect"
              class="md-select"
              .value="${this.selectedChannel_}"
              @change="${this.onChannelChange_}"
              @keydown="${this.onSelectKeydown_}"
              ?disabled="${this.disabled}">
            ${this.channels_.map(item => html`
              <option value="${item.channel}">
                ${item.displayName || item.channel}
              </option>
            `)}
          </select>
        </div>
      ` : ''}
    </div>
  `}
</div>
`;
}
