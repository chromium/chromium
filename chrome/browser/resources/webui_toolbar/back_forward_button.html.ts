// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {BackForwardButtonElement} from './back_forward_button.js';

export function getHtml(this: BackForwardButtonElement) {
  return html`<!--_html_template_start_-->
<cr-icon-button
    iron-icon="cr:arrow-back"
    ?disabled="${!this.state.enabled}"
    aria-label="${this.ariaLabel_}"
    title="${this.tooltip_}"
    style="margin-inline-start: ${this.leadingMargin}px"
    @pointerdown="${this.onPointerdown_}"
    @pointerup="${this.onPointerup_}"
    @pointercancel="${this.onPointercancel_}"
    @pointerenter="${this.onPointerenter_}"
    @contextmenu="${this.onContextmenu_}">
  </cr-icon-button>

<!--_html_template_end_-->`;
}
