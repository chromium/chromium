// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ViewerThumbnailBarElement} from './viewer_thumbnail_bar.js';

export function getHtml(this: ViewerThumbnailBarElement) {
  return html`
<div id="thumbnails" ?hidden="${!this.isPluginActive_}" role="tablist">
  ${this.computePageNumbers_().map(item => html`
    <viewer-thumbnail tabindex="0" role="tab"
        aria-label="${this.getAriaLabel_(item)}"
        aria-selected="${this.isActivePage_(item)}"
        .clockwiseRotations="${this.clockwiseRotations}"
        .isActive="${this.isActivePage_(item)}"
        .pageNumber="${item}">
    </viewer-thumbnail>`)}
</div>`;
}
