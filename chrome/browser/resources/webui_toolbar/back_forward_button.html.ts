// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {BackForwardButtonElement} from './back_forward_button.js';

export function getHtml(this: BackForwardButtonElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<!-- buttonWrapper expands the clickable hit target to window edges
  (Fitts' law). The title attribute is duplicated on buttonWrapper so hovering
  over the expanded padding area properly displays the native browser
  tooltip (browsers prioritize the identical inner button title when hovered
  directly). Note: Because <div> has no native disabled behavior, ?disabled
  is applied strictly as a CSS selector hook for styling disabled states. -->
<div id="buttonWrapper"
    title="${this.getTooltip_()}"
    ?disabled="${!this.state.enabled}"
    @pointerdown="${this.onPointerdown_}"
    @pointerup="${this.onPointerup_}"
    @pointercancel="${this.onPointercancel_}"
    @pointerleave="${this.onPointerleave_}"
    @pointerenter="${this.onPointerenter_}"
    @contextmenu="${this.pressHandler_.onContextmenu}"
    @click="${this.onClick_}">
  <!-- Disable standard ink ripple when Glow Up is enabled to avoid visual
    clashing with the interactive SMIL icon animation and glow styling. -->
  <cr-icon-button id="button"
      iron-icon="${this.getIronIcon_()}"
      ?disabled="${!this.state.enabled}"
      ?noink="${this.glowUpEnabled}"
      aria-label="${this.getAriaLabel_()}"
      title="${this.getTooltip_()}"
      ?is-menu-open="${this.state.isContextMenuVisible}">
  </cr-icon-button>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
