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

<p ?hidden="${!this.isIwaPolicyInstallEnabled_}">
  Isolated Web Apps developer tools have moved to
  <a href="chrome://iwa-dev">chrome://iwa-dev</a>.
</p>

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
