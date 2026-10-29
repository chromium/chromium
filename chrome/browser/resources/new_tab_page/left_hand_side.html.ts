// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {LeftHandSideElement} from './left_hand_side.js';

export function getHtml(this: LeftHandSideElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
  ${this.enableThreadsRailHybrid_
      ? html`<cr-threads-rail id="folded"
                 ?hidden="${this.isExpandedReady_ && this.isExpanded_}"
                 @click="${this.onFoldedClick_}">
             </cr-threads-rail>`
      : ''}
  <ntp-iframe id="expanded"
      class="${this.isExpandedReady_ ? 'ready' : ''}"
      allow="unbounded-element"
      style="${this.enableThreadsRailHybrid_
                 ? `width: ${this.expandedWidth_}px;`
                 : ''}"
      src="${this.getExpandedLhsUrl_()}"
      ?inert="${!this.isExpandedReady_}"
      ?hidden="${this.enableThreadsRailHybrid_ &&
                 (!this.isExpandedReady_ || !this.isExpanded_)}">
  </ntp-iframe>
  <!--_html_template_end_-->`;
  // clang-format on
}
