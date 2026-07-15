// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {IwaDevAppElement} from './app.js';

export function getHtml(this: IwaDevAppElement) {
  // clang-format off
  return html`
<div class="container">
  <h1>Isolated Web App Developer Tool</h1>
  ${!this.devModeEnabled ? html`
    <div id="error-message">
      <p>Isolated Web App Developer Mode is disabled.</p>
      <p>To use this page, please enable the
        <a href="chrome://flags#enable-isolated-web-app-dev-mode">
          Isolated Web App Developer Mode
        </a> flag.
      </p>
    </div>
  ` : ''}
</div>`;
}
