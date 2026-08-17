// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/shared/icon_from_table.js';
import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PageActionIconElement} from './page_action_icon.js';

export function getHtml(this: PageActionIconElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<toolbar-chip-button outset-focus-ring id="button"
    .buttonTabIndex="${0}"
    ?animates-label="${this.shouldAnimate_()}"
    ?has-label="${this.shouldShowLabel_()}"
    .tooltip="${this.state.tooltipText}"
    .ariaLabel="${this.getAriaLabel_()}"
    @click="${this.onClick_}"
    @pointerenter="${this.onPointerenter_}"
    @pointerleave="${this.onPointerleave_}"
    @pointercancel="${this.onPointercancel_}">
  <icon-from-table slot="prefix-icon" id="icon"
      .iconHandle="${this.state.icon}"></icon-from-table>
  <span id="text" ?visible="${this.shouldShowLabel_()}">
    ${this.state.text || ''}
  </span>
</toolbar-chip-button>
<!--_html_template_end_-->`;
  // clang-format on
}
