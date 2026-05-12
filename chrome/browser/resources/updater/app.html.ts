// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {PageDataSource} from './app.js';
import type {UpdaterAppElement} from './app.js';

export function getHtml(this: UpdaterAppElement) {
  // clang-format off
  return html`
<!--_html_template_start_-->
<header>
  <div class="header-bar">
    <div id="logo"></div>
    <h1>$i18n{title}</h1>
    <div id="controls">
      <div class="file-selection-banner" ?error="${this.historyLoadError}">
        ${this.historyLoadError ? html`
          <span>$i18n{loadHistoryFileError}</span>
        ` : ''}
        ${this.pageDataSource === PageDataSource.FILE ? html`
          <span>${this.fileSelectionBannerLabel}</span>
          <cr-button @click="${this.onCloseFileClick}">
            $i18n{returnToLocal}
          </cr-button>
        ` : html`
          <a href="chrome://support-tool/?module=CgEd" target="_blank"
              rel="noopener noreferrer">
            <cr-button>
              $i18n{exportHistoryFile}
            </cr-button>
          </a>
          <cr-button @click="${this.onLoadHistoryClick}">
            $i18n{loadHistoryFile}
          </cr-button>
          <a href="https://support.google.com/chrome/a/answer/17070626"
              target="_blank" rel="noopener noreferrer">
            <cr-button title="$i18n{helpCenterTooltip}">
              $i18n{learnMore}
            </cr-button>
          </a>
        `}
        <input type="file" id="fileInput" hidden multiple
            accept=".jsonl, .jsonl.old, .zip"
            @change="${this.onFileInputChange}">
      </div>
    </div>
  </div>
</header>
<div id="content">
  ${this.pageDataSource === PageDataSource.INSTALL ? html`
    <div>
      <h2>$i18n{updaterStateTitle}</h2>
      <updater-state .userUpdaterState="${this.userUpdaterState}"
          .systemUpdaterState="${this.systemUpdaterState}"
          .error="${this.updaterStateError}">
      </updater-state>
    </div>
  ` : ''}
  <div>
    <h2>$i18n{installedAppsTitle}</h2>
    <app-list .apps="${this.apps}" .error="${this.appStateError}"></app-list>
  </div>
  <div>
    <h2>Enterprise Policies</h2>
    <enterprise-policy-table .policies="${this.policies}">
    </enterprise-policy-table>
  </div>
  <div>
    <h2>$i18n{eventListTitle}</h2>
    <event-list .messages="${this.messages}"></event-list>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
