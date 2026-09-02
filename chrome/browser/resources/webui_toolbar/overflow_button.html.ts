// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OverflowButtonElement} from './overflow_button.js';

export function getHtml(this: OverflowButtonElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-icon-button
    class="iph-visual-target"
    iron-icon="webui-toolbar:keyboard_double_arrow_right"
    noink
    @pointerdown="${this.onPointerdown_}"
    @click="${this.onClick_}"
    title="${this.getTooltip_()}"
    aria-label="${this.getLabel_()}"
    aria-haspopup="menu"
    aria-expanded="${this.state.isContextMenuVisible}"
    ?is-menu-open="${this.state.isContextMenuVisible}">
</cr-icon-button>
<!--_html_template_end_-->`;
  // clang-format on
}
