// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {PermissionChipElement} from './permission_chip.js';

export function getHtml(this: PermissionChipElement) {
  // clang-format off
  return (!this.chipState || !this.chipState.isVisible) ? nothing :
      html`<!--_html_template_start_-->
<div id="chip"
    ?collapsed="${this.chipState.isFullyCollapsed}"
    aria-label="${this.chipState.accessibilityName}"
    title="${this.chipState.tooltip}"
    @pointerenter="${this.onPointerenter_}"
    @pointerleave="${this.onPointerleave_}"
    @pointercancel="${this.onPointercancel_}"
    @pointerdown="${this.onPointerdown_}"
    @click="${this.onClick_}">
  ${this.getIconUrl_() ? html`
    <div id="icon"
        style="mask-image: ${this.getIconUrl_()};">
    </div>` : nothing}
  ${!this.chipState.isFullyCollapsed ? html`
    <span id="message">${this.chipState.message}</span>` : nothing}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
