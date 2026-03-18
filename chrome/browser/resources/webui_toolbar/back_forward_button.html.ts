// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {BackForwardButtonElement} from './back_forward_button.js';

export function getHtml(this: BackForwardButtonElement) {
  return html`<!--_html_template_start_-->
<cr-icon-button
    iron-icon="${
      this.direction === 'back' ? 'cr:arrow-back' : 'cr:arrow-forward'}"
    ?disabled="${!this.state.enabled}"
    aria-label="${this.ariaLabel_}"
    title="${this.tooltip_}"
    style="margin-inline-start: ${this.leadingMargin}px"
    @pointerdown="${this.pressHandler_.onPointerdown}"
    @pointerup="${this.pressHandler_.onPointerup}"
    @pointercancel="${this.pressHandler_.onPointercancel}"
    @pointerenter="${this.onPointerenter_}"
    @contextmenu="${this.pressHandler_.onContextmenu}">
  </cr-icon-button>

<!--_html_template_end_-->`;
}
