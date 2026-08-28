// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OverflowButtonElement} from './overflow_button.js';

export function getHtml(this: OverflowButtonElement) {
  // TODO(crbug.com/491791965): Add accessibility fields and localize, adding
  // tooltip, ariaLabel, is-menu-open, ariaHasPopup, and ariaExpanded.

  // clang-format off
  return html`<!--_html_template_start_-->
<cr-icon-button
    class="iph-visual-target"
    iron-icon="webui-toolbar:keyboard_double_arrow_right"
    noink
    @pointerdown="${this.onPointerdown_}"
    @click="${this.onClick_}">
</cr-icon-button>
<!--_html_template_end_-->`;
  // clang-format on
}
