// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './install_dev_proxy_tab.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {TabIndex} from './install_dialog.js';
import type {IwaDevInstallDialogElement} from './install_dialog.js';

export function getHtml(this: IwaDevInstallDialogElement) {
  // clang-format off
  return html`
<cr-dialog id="dialog">
  <div slot="title">Install Isolated Web App</div>
  <div slot="body">
    <cr-tabs
        .tabNames="${['Dev Mode Proxy', 'Signed Bundle', 'Update Manifest']}"
        .selected="${this.selectedTab_}"
        @selected-changed="${this.onSelectedChanged_}"
        ?inert="${this.isInstalling_}">
    </cr-tabs>
    <div class="tab-content">
      ${this.isOpened_ && this.selectedTab_ === TabIndex.DEV_PROXY ? html`
        <iwa-dev-install-dev-proxy-tab
            ?disabled="${this.isInstalling_}"
            @valid-changed="${this.onTabValidChanged_}">
        </iwa-dev-install-dev-proxy-tab>
      ` : html`
        <p>Not implemented yet.</p>
      `}
      ${this.installationError_ ? html`
        <div id="error-message" aria-live="polite">
          ${this.installationError_}
        </div>
      ` : ''}
    </div>
  </div>
  <div slot="button-container">
    <cr-button class="cancel-button" @click="${this.onCancelClick_}"
        ?disabled="${this.isInstalling_}">
      Cancel
    </cr-button>
    <cr-button class="action-button"
        @click="${this.onInstallClick_}"
        ?disabled="${!this.isInstallButtonEnabled_()}">
      ${this.isInstalling_ ? 'Installing...' : 'Install'}
    </cr-button>
  </div>
</cr-dialog>`;
}
