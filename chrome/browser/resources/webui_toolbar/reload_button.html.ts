// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ReloadButtonAppElement} from './reload_button.js';

export function getHtml(this: ReloadButtonAppElement) {
  return html`<!--_html_template_start_-->
<cr-icon-button class="${
      this.state.isNavigationLoading ? 'icon-clear' : 'icon-refresh'}"
    title="${this.tooltip}"
    aria-label="${this.accName_}"
    aria-haspopup="${this.state.isDevtoolsConnected}"
    is-menu-open="${this.state.isContextMenuVisible}"
    @pointerdown="${this.onReloadButtonPointerdown_}"
    @pointerup="${this.onReloadButtonPointerup_}"
    @contextmenu="${this.onContextmenu_}">
</cr-icon-button>
<!--_html_template_end_-->`;
}
