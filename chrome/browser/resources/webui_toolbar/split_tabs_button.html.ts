// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SplitTabsButtonElement} from './split_tabs_button.js';

export function getHtml(this: SplitTabsButtonElement) {
  return html`<!--_html_template_start_-->
<cr-icon-button id="button" iron-icon="${this.getIcon()}"
    @click="${this.onClick}" @contextmenu="${this.onContextmenu}"
    title="${this.getLabel()}" aria-label="${this.getLabel()}"
    aria-haspopup="${this.state.isCurrentTabSplit ? 'menu' : 'false'}"
    is-menu-open="${this.state.isContextMenuVisible}"
    is-split="${this.state.isCurrentTabSplit}">
</cr-icon-button>
<div class="status-indicator" ?hidden="${!this.state.isCurrentTabSplit}"></div>
<!--_html_template_end_-->`;
}
