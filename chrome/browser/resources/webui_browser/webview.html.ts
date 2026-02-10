// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {WebviewElement} from './webview.js';

export function getHtml(this: WebviewElement) {
  // clang-format off
  return html`
${this.enableSurfaceEmbed ? html`
  <embed class="content" type="application/x-chromium-surface-embed"
      data-content-id="${this.guestId}">
  </embed>
` : html`
  <iframe class="content"></iframe>
`}
`;
  // clang-format on
}
