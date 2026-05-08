// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContentSettingIconElement} from './content_setting_icon.js';

export function getHtml(this: ContentSettingIconElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<!-- TODO: crbug.com/489109708 - Fix anchor highlights. -->
<cr-icon-button id="button" class="${this.getIconClass_()}"
    title="${this.state.tooltip}" aria-label="${this.getAriaLabel_()}"
    @click="${this.onClick_}" @auxclick="${this.onAuxclick_}"
    @contextmenu="${this.onContextmenu_}"
    @pointerenter="${this.onPointerenter_}"
    @pointerleave="${this.onPointerleave_}"
    @pointercancel="${this.onPointercancel_}">
</cr-icon-button>
<!-- TODO: crbug.com/489109708 - The animation text should actually be part of
           the button (clicks should active bubble), and pause animation. -->
<div id="label" @animationend="${this.onLabelAnimationend_}">
  ${this.state.explanatoryString}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
