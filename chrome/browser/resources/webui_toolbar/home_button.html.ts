// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {HomeButtonElement} from './home_button.js';

export function getHtml(this: HomeButtonElement) {
  return html`<!--_html_template_start_-->
<cr-icon-button id="button" iron-icon="home-button:navigate-home"
    @pointerdown="${this.pressHandler_.onPointerdown}"
    @pointerup="${this.pressHandler_.onPointerup}"
    @pointercancel="${this.pressHandler_.onPointercancel}"
    @contextmenu="${this.pressHandler_.onContextmenu}"
    title="${this.tooltip_}" aria-label="${this.label_}"
    is-menu-open="${this.state.isContextMenuVisible}">
</cr-icon-button>
<!--_html_template_end_-->`;
}
