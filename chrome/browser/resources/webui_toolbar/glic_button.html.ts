// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {GlicButtonElement} from './glic_button.js';

export function getHtml(this: GlicButtonElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<toolbar-chip-button id="button"
    class="${this.hasHelpBubble ? 'help-anchor-highlight' : ''}"
    ?has-label="${!!this.getLabel_()}"
    ?is-menu-open="${this.state.open || this.state.isContextMenuVisible}"
    .disabled="${!this.enabled}"
    .tooltip="${this.getTooltip_()}"
    .ariaLabel="${this.getAriaLabel_()}"
    .ariaExpanded="${this.state.open ? 'true' : 'false'}"
    .ariaHasPopup="${'dialog'}"
    @click="${this.onClick_}"
    @contextmenu="${this.onContextmenu_}">
  ${this.state.open ? html`
    <cr-icon slot="prefix-icon" id="icon"
        icon="webui-toolbar:glic_button">
    </cr-icon>
  ` : html`
    <img slot="prefix-icon" id="icon"
        src="images/glic_button_alt_icon.png" alt="">
  `}
  ${this.getLabel_() ? html`<span id="text">${this.getLabel_()}</span>` : ''}
</toolbar-chip-button>
<!--_html_template_end_-->`;
  // clang-format on
}
