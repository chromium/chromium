// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContentSettingIconElement} from './content_setting_icon.js';

export function getHtml(this: ContentSettingIconElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<toolbar-chip-button outset-focus-ring animates-label id="chip"
    .buttonTabIndex="${0}"
    ?is-menu-open="${this.trackedHighlighted}"
    ?has-label="${this.shouldShowLabel_ && !!this.state.explanatoryString}"
    ?disable-transitions="${this.suppressTransitions_}"
    .tooltip="${this.state.tooltip}" .ariaLabel="${this.getAriaLabel_()}"
    @click="${this.onClick_}" @auxclick="${this.onAuxclick_}"
    @contextmenu="${this.onContextmenu_}"
    @pointerenter="${this.onPointerenter_}"
    @pointerleave="${this.onPointerleave_}"
    @pointercancel="${this.onPointercancel_}"
    @pointerdown="${this.onPointerdown_}"
    @pointerup="${this.onPointerup_}">
  <cr-icon id="icon" slot="prefix-icon"
      .icon="${this.getIconName_()}"></cr-icon>
  <span id="label">${this.state.explanatoryString}</span>
</toolbar-chip-button>
<!--_html_template_end_-->`;
  // clang-format on
}
