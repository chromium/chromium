// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {WebuiBrowserAppElement} from './app.js';

export function getHtml(this: WebuiBrowserAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<h1>WebUI Browser
 <cr-button type="button" @click="${this.onMinimizeClick_}">[-]</cr-button>
 <cr-button type="button" @click="${this.onMaximizeClick_}">[+]</cr-button>
 <cr-button type="button" @click="${this.onCloseClick_}">[X]</cr-button>
</h1>
<div id="exampleDiv">${this.message_}</div>
<cr-webview id="exampleWebview" guest-id="${this.guestId_}"></cr-webview>
<!--_html_template_end_-->`;
  // clang-format on
}
