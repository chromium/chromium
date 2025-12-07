// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ViewerThumbnailElement} from './viewer_thumbnail.js';

export function getHtml(this: ViewerThumbnailElement) {
  return html`
<div id="thumbnail" @click="${this.onClick_}" role="button">
  <div id="canvas-container"></div>
</div>
<div id="pageNumber">${this.pageNumber}</div>`;
}
