// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContentSettingIconElement} from './content_setting_icon.js';

export function getHtml(this: ContentSettingIconElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<!-- TODO: crbug.com/489109708 - Fix anchor highlights. -->
<toolbar-chip-button id="chip"
    ?has-label="${Boolean(this.state.explanatoryString)}"
    .tooltip="${this.state.tooltip}" .ariaLabel="${this.getAriaLabel_()}"
    @click="${this.onClick_}" @auxclick="${this.onAuxclick_}"
    @contextmenu="${this.onContextmenu_}"
    @pointerenter="${this.onPointerenter_}"
    @pointerleave="${this.onPointerleave_}"
    @pointercancel="${this.onPointercancel_}"
    @pointerdown="${this.onPointerdown_}">
  <div id="icon" slot="prefix-icon"
      style="mask-image: ${this.getIconUrl_()};"></div>
  <span id="label">${this.state.explanatoryString}</span>
</toolbar-chip-button>
<!--_html_template_end_-->`;
  // clang-format on
}
