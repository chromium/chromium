// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {LhsGhostLoaderElement} from './lhs_ghost_loader.js';

export function getHtml(this: LhsGhostLoaderElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="ghost-header" aria-hidden="true">
  <div class="ghost-logo">
    <div class="ghost-wordmark"></div>
  </div>
  <div class="ghost-close-button">
    <div class="ghost-icon"></div>
  </div>
  <div class="ghost-actions">
    <div class="ghost-button">
      <div class="ghost-icon"></div>
      <div class="ghost-label new-thread"></div>
    </div>
    <div class="ghost-button">
      <div class="ghost-icon"></div>
      <div class="ghost-label search-threads"></div>
    </div>
  </div>
</div>
<div class="ghost-threads" aria-hidden="true">
  ${this.ghostLoadingBars_.map(bar => html`
    <div class="ghost-line-item">
      <div class="ghost-line"
          style="--loading-bar-width: ${bar.width};
                 height: ${bar.height};
                 border-radius: ${bar.borderRadius};">
        ${bar.layerDelaysMs.map(delay => html`
          <div class="ghost-shimmer"
              style="--loading-bar-delay: ${delay}ms;">
          </div>`)}
      </div>
    </div>`)}
</div>
<div class="ghost-footer" aria-hidden="true">
  <div class="ghost-button">
    <div class="ghost-icon"></div>
    <div class="ghost-label settings"></div>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
