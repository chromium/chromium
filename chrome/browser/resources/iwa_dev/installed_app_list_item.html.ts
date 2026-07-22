// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {InstalledAppListItemElement} from './installed_app_list_item.js';

export function getHtml(this: InstalledAppListItemElement) {
  // clang-format off
  return html`
<div id="icon-circle">
  ${this.app.name.length > 0 ? html`
    <img src="chrome://app-icon/${this.app.appId}/48" alt="">
  ` : ''}
</div>
<div id="details">
  <div id="header">
    <span id="name">${this.app.name}</span>
    <span id="version">v${this.app.installedVersion}</span>
    <span id="source-text">• ${this.sourceMetadata.label}</span>
  </div>
  <div id="id">ID: ${this.app.appId}</div>
  <div id="source">${this.sourceMetadata.description}</div>
</div>
<cr-button id="uninstall-btn" @click="${this.onUninstallClick}">
  Uninstall
</cr-button>
`;
}
