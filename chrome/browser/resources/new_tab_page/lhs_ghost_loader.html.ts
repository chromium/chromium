// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {LhsGhostLoaderElement} from './lhs_ghost_loader.js';

export function getHtml(this: LhsGhostLoaderElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="ghost-header">
  <div class="ghost-logo" aria-hidden="true">
    <div class="ghost-wordmark">
      ${this.shimmerDelaysMs_.logo.map(item => html`
        <div class="ghost-shimmer" style="--loading-bar-delay: ${item}ms;">
        </div>
      `)}
    </div>
  </div>
  <cr-icon-button id="closeButton" @click="${this.onCloseClick_}"
      aria-label="${this.closeLabel_}">
  </cr-icon-button>
  <div class="ghost-actions" aria-hidden="true">
    <div class="ghost-button">
      <div class="ghost-icon">
        ${this.shimmerDelaysMs_.newThread.map(item => html`
          <div class="ghost-shimmer" style="--loading-bar-delay: ${item}ms;">
          </div>
        `)}
      </div>
      <div class="ghost-label new-thread">
        ${this.shimmerDelaysMs_.newThread.map(item => html`
          <div class="ghost-shimmer" style="--loading-bar-delay: ${item}ms;">
          </div>
        `)}
      </div>
    </div>
    <div class="ghost-button">
      <div class="ghost-icon">
        ${this.shimmerDelaysMs_.searchThreads.map(item => html`
          <div class="ghost-shimmer" style="--loading-bar-delay: ${item}ms;">
          </div>
        `)}
      </div>
      <div class="ghost-label search-threads">
        ${this.shimmerDelaysMs_.searchThreads.map(item => html`
          <div class="ghost-shimmer" style="--loading-bar-delay: ${item}ms;">
          </div>
        `)}
      </div>
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
          <div class="ghost-shimmer" style="--loading-bar-delay: ${delay}ms;">
          </div>
        `)}
      </div>
    </div>
  `)}
</div>
<div class="ghost-footer" aria-hidden="true">
  <div class="ghost-button">
    <div class="ghost-icon">
      ${this.shimmerDelaysMs_.settings.map(item => html`
        <div class="ghost-shimmer" style="--loading-bar-delay: ${item}ms;">
        </div>
      `)}
    </div>
    <div class="ghost-label settings">
      ${this.shimmerDelaysMs_.settings.map(item => html`
        <div class="ghost-shimmer" style="--loading-bar-delay: ${item}ms;">
        </div>
      `)}
    </div>
  </div>
</div>
<!-- NTP: outside .ghost-header, which is position: relative, so that the
     tooltip is positioned against an ancestor of <ntp-lhs-ghost-loader> and
     is not clipped by its overflow: hidden. -->
<cr-tooltip id="closeTooltip" for="closeButton"
    position="${this.closeTooltipPosition_}" offset="16" animation-delay="0"
    hide-delay="0">
  ${this.closeLabel_}
</cr-tooltip>
<!--_html_template_end_-->`;
  // clang-format on
}
