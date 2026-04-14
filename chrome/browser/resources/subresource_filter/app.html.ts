// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SubresourceInternalsAppElement} from './app.js';

export function getHtml(this: SubresourceInternalsAppElement) {
  return html`
<header>
  <h1>Subresource Filter Internals</h1>
</header>
<div id="content">
  <section class="card">
    <h2>Settings</h2>
    <div class="row">
      <div class="row-text">
        <div>Highlight ads</div>
        <div class="secondary-text">
          Highlight elements (red) detected to be ads.
        </div>
      </div>
      <cr-checkbox id="highlight-ads-checkbox" class="no-label"
          .checked="${this.shouldHighlightAds_}"
          @change="${this.onHighlightAdsCheckboxChange_}">
      </cr-checkbox>
    </div>
  </section>
</div>
  `;
}
