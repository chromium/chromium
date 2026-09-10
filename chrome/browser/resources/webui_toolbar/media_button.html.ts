// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {MediaButtonElement} from './media_button.js';

export function getHtml(this: MediaButtonElement) {
  return html`<!--_html_template_start_-->
<cr-icon-button id="button" iron-icon="webui-toolbar:queue_music"
    @pointerdown="${this.onPointerdown_}"
    @pointerup="${this.pressHandler_.onPointerup}"
    @pointercancel="${this.pressHandler_.onPointercancel}"
    @click="${this.onClick_}"
    @contextmenu="${this.pressHandler_.onContextmenu}"
    title="${this.getTooltip_()}"
    ?disabled="${!this.state.enabled}"
    ?is-menu-open="${this.state.isContextMenuVisible}">
</cr-icon-button>
<!--_html_template_end_-->`;
}
