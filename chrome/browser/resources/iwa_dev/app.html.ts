// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {IwaDevAppElement} from './app.js';

export function getHtml(this: IwaDevAppElement) {
  // clang-format off
  return html`
<h1>Isolated Web App Developer Tool</h1>
${!this.devModeEnabled_ ? html`
  <div id="error-message">
    <p>Isolated Web App Developer Mode is disabled.</p>
    <p>To use this page, please enable the
      <a href="chrome://flags#enable-isolated-web-app-dev-mode">
        Isolated Web App Developer Mode
      </a> flag.
    </p>
  </div>
` : html`
  <div id="content">
    ${!this.hasFetchedApps_ ? '' : html`
      <h2>
        Installed Applications (${this.installedApps_.length})
      </h2>
      ${this.installedApps_.length === 0 ? html`
        <div id="iwa-list-message">
          No Isolated Web Apps installed in developer mode.
        </div>
      ` : html`
        <div id="iwa-list" role="list">
        ${this.installedApps_.map(item => html`
          <installed-app-list-item
              .app="${item}"
              role="listitem"
              @request-uninstall="${this.onRequestUninstall}">
          </installed-app-list-item>
        `)}
        </div>
      `}
    `}
  </div>
`}
`;
}
