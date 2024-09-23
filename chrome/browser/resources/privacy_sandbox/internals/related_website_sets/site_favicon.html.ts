// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SiteFaviconElement} from './site_favicon.js';

export function getHtml(this: SiteFaviconElement) {
  return html`
    <div id="favicon" .style="background-image: ${this.getBackgroundImage_()}"
        ?hidden="${this.showDownloadedIcon_}">
    </div>
    <img is="cr-auto-img" id="downloadedFavicon"
        ?hidden="${!this.showDownloadedIcon_}" @load="${this.onLoadSuccess_}"
        @error="${this.onLoadError_}" .autoSrc="${this.url}">`;
}
