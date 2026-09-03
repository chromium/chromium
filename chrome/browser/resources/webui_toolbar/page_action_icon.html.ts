// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/shared/icon_from_table.js';
import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {PageActionIconElement} from './page_action_icon.js';

export function getHtml(this: PageActionIconElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<toolbar-chip-button outset-focus-ring id="button"
    style="${this.chipStyleOverride_ ?? nothing}"
    .buttonTabIndex="${0}"
    ?is-menu-open="${this.state.isActive || this.isHighlighted}"
    ?animates-label="${this.shouldAnimate_()}"
    ?has-label="${this.shouldShowLabel_()}"
    .tooltip="${this.getTooltip_()}"
    .ariaLabel="${this.getAriaLabel_()}"
    @click="${this.onClick_}"
    @pointerdown="${this.onPointerdown_}"
    @pointerenter="${this.onPointerenter_}"
    @pointerleave="${this.onPointerleave_}"
    @pointercancel="${this.onPointercancel_}">
  ${this.isIconAnimating_() ? html`
    <cr-icon slot="prefix-icon" id="animatedIcon"
        style="${this.getAnimatedIconStyle_() ?? nothing}"
        .icon="${this.getAnimatedIcon_()}"></cr-icon>
  ` : html`
    <icon-from-table slot="prefix-icon" id="icon"
        .iconHandle="${this.state.icon}"></icon-from-table>
  `}
  <span id="text" ?visible="${this.shouldShowLabel_()}">
    ${this.state.text || ''}
  </span>
</toolbar-chip-button>
<!--_html_template_end_-->`;
  // clang-format on
}
