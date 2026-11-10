// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/icons.html.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AvatarButtonElement} from './avatar_button.js';

export function getHtml(this: AvatarButtonElement) {
  return html`<!--_html_template_start_-->
<toolbar-chip-button id="button" class="${this.getButtonClass_()}"
    ?outset-focus-ring="${!!this.state.text}"
    ?has-border="${this.shouldPaintBorder()}"
    ?has-linear-gradient-ring="${this.state.hasLinearGradientRing}"
    .buttonTabIndex="${0}" .disabled="${!this.state.enabled}"
    .tooltip="${this.getTooltip_() || ''}"
    .ariaLabel="${this.state.accessibilityName || ''}" .ariaHasPopup="${'menu'}"
    .ariaDescription="${this.state.accessibilityDescription || ''}"
    @click="${this.onClick_}"
    @pointerdown="${this.highlightTracker.onPointerdown}"
    @mouseenter="${this.onMouseenter_}" @mouseleave="${this.onMouseleave_}"
    @focusin="${this.onFocusin_}" @focusout="${this.onFocusout_}">
  <span slot="prefix-icon" class="icon-spacer" id="icon-spacer-start"></span>
  ${(this.state.icon?.handleId ?? 0n) !== 0n ? html`
    <icon-from-table slot="prefix-icon" id="icon"
        .iconHandle="${this.state.icon}">
    </icon-from-table>
  ` : html`
    <cr-icon slot="prefix-icon" id="icon" icon="cr:person-filled"></cr-icon>
  `}
  <span slot="prefix-icon" class="icon-spacer"></span>
  <span id="text" ?visible="${!!this.state.text}"
      .textContent="${this.state.text || ''}">
  </span>
  <span id="ellipsis" aria-hidden="true">…</span>
</toolbar-chip-button>
<!--_html_template_end_-->`;
}
