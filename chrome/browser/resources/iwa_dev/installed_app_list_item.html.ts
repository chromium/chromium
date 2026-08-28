// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {InstalledAppListItemElement} from './installed_app_list_item.js';

export function getHtml(this: InstalledAppListItemElement) {
  // clang-format off
  return html`
<div id="icon-circle">
  ${this.app.name.length > 0 ? html`
    <img id="app-icon" draggable="false"
         src="chrome://app-icon/${this.app.appId}/48" alt="">
  ` : ''}
</div>
<div id="details">
  <div id="header">
    <span id="name">${this.app.name}</span>
    <span id="version">v${this.app.installedVersion}</span>
    <span id="source-text">• ${this.sourceMetadata.label}</span>
  </div>
  <div id="id">ID: ${this.app.webBundleId}</div>
  <div id="source">
    ${this.sourceMetadata.description}
    ${this.isManifestApp_() ? html`
      <cr-icon-button id="copy-btn"
          class="${this.copied_ ? '' : 'icon-copy-content'}"
          iron-icon="${this.copied_ ? 'cr:check' : nothing}"
          aria-label="${this.copied_ ? 'Copied' : 'Copy update manifest URL'}"
          title="${this.copied_ ? 'Copied' : 'Copy update manifest URL'}"
          @click="${this.onCopyClick}">
      </cr-icon-button>
    ` : ''}
  </div>
</div>
<div id="actions">
  ${this.isManifestApp_() ? html`
    <div id="split-button">
      <cr-button id="update-btn" ?disabled="${this.isUpdating}"
          @click="${this.onUpdateClick}">
        Update
      </cr-button>
      <cr-button id="update-options-btn"
          aria-label="Update options"
          title="Update options"
          ?disabled="${this.isUpdating}"
          @click="${this.onUpdateOptionsClick}">
        <cr-icon class="icon-16" icon="cr:settings-filled"></cr-icon>
      </cr-button>
    </div>
  ` : html`
    <cr-button id="update-btn" ?disabled="${this.isUpdating}"
        @click="${this.onUpdateClick}">
      Update
    </cr-button>
  `}
  <cr-button id="uninstall-btn" @click="${this.onUninstallClick}">
    Uninstall
  </cr-button>
</div>
`;
}
