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
    Manifest loaded successfully: ${this.versionOptions_.length} version${
        this.versionOptions_.length === 1 ? '' : 's'} available.
  </div>
` : ''}
<div id="details">
  ${!this.isManifestFetched_ ? html`
    <div class="placeholder-message">
      Enter a URL and click Fetch to load manifest details.
    </div>
  ` : html`
    <div id="dropdowns-row">
      <iwa-dev-combobox id="versionSelect"
          label="Version"
          readonly
          .value="${this.selectedVersion_}"
          .options="${this.versionOptions_}"
          @value-changed="${this.onVersionValueChanged_}"
          @keydown="${this.onSelectKeydown_}"
          ?disabled="${this.disabled}">
      </iwa-dev-combobox>
      ${this.channelOptions_.length > 0 ? html`
        <iwa-dev-combobox id="channelSelect"
            label="Update Channel"
            readonly
            .value="${this.selectedChannel_}"
            .options="${this.channelOptions_}"
            @value-changed="${this.onChannelValueChanged_}"
            @keydown="${this.onSelectKeydown_}"
            ?disabled="${this.disabled}">
        </iwa-dev-combobox>
      ` : ''}
    </div>
  `}
</div>
`;
}
