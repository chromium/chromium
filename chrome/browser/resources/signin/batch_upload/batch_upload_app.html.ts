// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {BatchUploadAppElement} from './batch_upload_app.js';

export function getHtml(this: BatchUploadAppElement) {
  return html`
<h1>Hello World</h1>
<div id="example-div">${this.message_}</div>
<cr-button @click=${this.close_}>Close</cr-button>`;
}
