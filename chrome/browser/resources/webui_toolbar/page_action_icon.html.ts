// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {PageActionIconElement} from './page_action_icon.js';

export function getHtml(this: PageActionIconElement) {
  return html`<!--_html_template_start_-->
<cr-icon-button id="button"
    iron-icon="${this.getIronIcon_() ?? nothing}"
    style="${this.getIconStyle_() ?? nothing}"
    title="${this.state.tooltipText}"
    aria-label="${this.getAriaLabel_()}"
    @click="${this.onClick_}"
    @pointerenter="${this.onPointerenter_}"
    @pointerleave="${this.onPointerleave_}"
    @pointercancel="${this.onPointercancel_}">
</cr-icon-button>
<!--_html_template_end_-->`;
}
