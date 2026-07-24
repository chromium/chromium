// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {PermissionChipElement} from './permission_chip.js';

export function getHtml(this: PermissionChipElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<button id="chip" type="button"
    role="button"
    tabindex="0"
    ?collapsed="${this.isFullyCollapsed_}"
    aria-label="${this.chipState?.accessibilityName || ''}"
    title="${this.chipState?.tooltip || ''}"
    @pointerenter="${this.onPointerenter_}"
    @pointerleave="${this.onPointerleave_}"
    @pointercancel="${this.onPointercancel_}"
    @pointerdown="${this.onPointerdown_}"
    @click="${this.onClick_}">
  ${this.getIconUrl_() ? html`
    <div id="icon"
        style="mask-image: ${this.getIconUrl_()};">
    </div>` : nothing}
  <span id="message" ?visible="${!this.isFullyCollapsed_}"
  >${this.chipState?.message || ''}</span>
</button>
<!--_html_template_end_-->`;
  // clang-format on
}
