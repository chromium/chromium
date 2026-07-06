// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AppMenuButtonElement} from './app_menu_button.js';

export function getHtml(this: AppMenuButtonElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<toolbar-chip-button id="button" class="${this.getHighlightClass_()}"
    ?is-menu-open="${this.state.isContextMenuVisible}"
    ?has-label="${!!this.state.labelText}"
    @pointerdown="${this.onPointerdown_}" @click="${this.onClick_}"
    @focusin="${this.onFocusin_}" @focusout="${this.onFocusout_}"
    .ariaLabel="${this.state.accessibilityText}"
    .ariaHasPopup="${'menu'}"
    .tooltip="${this.getTooltip_()}"
    style="--toolbar-chip-trailing-margin: ${this.state.trailingMargin}px">
  ${this.state.labelText ? html`<span>${this.state.labelText}</span>` : ''}
  <cr-icon id="icon" icon="cr:more-vert" slot="suffix-icon"></cr-icon>
</toolbar-chip-button>
<!--_html_template_end_-->`;
  // clang-format on
}
