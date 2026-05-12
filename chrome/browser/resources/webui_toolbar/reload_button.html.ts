// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ReloadButtonElement} from './reload_button.js';

export function getHtml(this: ReloadButtonElement) {
  return html`<!--_html_template_start_-->
<cr-icon-button
    class="${this.showStopIcon ? 'icon-clear' : 'icon-refresh'}"
    title="${this.tooltip}"
    aria-label="${this.accName_}"
    aria-haspopup="${this.state.canShowMenu}"
    ?disabled="${this.isDisabled}"
    ?is-menu-open="${this.state.isContextMenuVisible}"
    @pointerdown="${this.onPointerdown_}"
    @pointerup="${this.onPointerup_}"
    @pointercancel="${this.pressHandler_.onPointercancel}"
    @contextmenu="${this.pressHandler_.onContextmenu}"
    @click="${this.onClick_}">
</cr-icon-button>
<!--_html_template_end_-->`;
}
