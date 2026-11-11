// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {LeftHandSideElement} from './left_hand_side.js';

export function getHtml(this: LeftHandSideElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
  ${this.enableThreadsRailHybrid_
      ? html`<cr-threads-rail id="folded"
                 ?inert="${this.isExpandedReady_ && this.isExpanded_}"
                 @expand-rail="${this.onExpandRail_}"
                 @click="${this.onFoldedClick_}"
                 @pointerenter="${this.onFoldedPointerenter_}"
                 @pointerleave="${this.onFoldedPointerleave_}"
                 @focusin="${this.onFoldedFocusin_}"
                 @focusout="${this.onFoldedFocusout_}">
             </cr-threads-rail>`
      : ''}
  ${!this.enableThreadsRailHybrid_ || this.hasPreloaded_
      ? html`<ntp-iframe id="expanded"
            class="${this.isExpandedReady_ ? 'ready' : ''}"
            allow="clipboard-write; unbounded-element"
            style="${this.enableThreadsRailHybrid_
                       ? `width: ${this.expandedWidth_}px;`
                       : ''}"
            src="${this.expandedLhsUrl_}"
            ?hidden="${!this.isExpandedReady_}"
            ?inert="${this.enableThreadsRailHybrid_ && !this.isExpanded_}">
        </ntp-iframe>`
      : ''}
  <!--_html_template_end_-->`;
  // clang-format on
}
