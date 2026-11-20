// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {LeftHandSideElement} from './left_hand_side.js';
import {LhsView} from './left_hand_side.js';

export function getHtml(this: LeftHandSideElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-threads-rail id="folded" ?inert="${this.view_ !== LhsView.FOLDED}"
    @expand-rail="${this.onExpandRail_}" @click="${this.onFoldedClick_}"
    @pointerenter="${this.onFoldedPointerenter_}"
    @pointerleave="${this.onFoldedPointerleave_}"
    @focusin="${this.onFoldedFocusin_}" @focusout="${this.onFoldedFocusout_}">
</cr-threads-rail>
${this.isExpanded_ ? html`
  <!-- Setting tabindex="-1" on <ntp-lhs-ghost-loader> would remove its close
       button from the tab order, so put it on a wrapper div instead. -->
  <div id="ghostLoader" role="region" aria-label="${this.lhsLabel_}"
      aria-busy="true" tabindex="-1"
      ?inert="${this.view_ !== LhsView.GHOST_LOADER}"
      style="width: ${this.expandedWidth_}px;">
    <ntp-lhs-ghost-loader ?is-dark="${!!this.isDark}"
        @close-rail="${this.onGhostLoaderCloseRail_}">
    </ntp-lhs-ghost-loader>
  </div>
` : ''}
${this.preloadStartTime_ !== null ? html`
  <ntp-iframe id="expanded" class="${this.isExpandedReady_ ? 'ready' : ''}"
      allow="clipboard-write; unbounded-element" .a11yLabel="${this.lhsLabel_}"
      style="width: ${this.expandedWidth_}px;" src="${this.expandedLhsUrl_}"
      ?hidden="${!this.isExpandedReady_}"
      ?inert="${this.view_ !== LhsView.EXPANDED}">
  </ntp-iframe>
` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
