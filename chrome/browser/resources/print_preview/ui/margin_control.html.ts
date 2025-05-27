// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {MarginControlElement} from './margin_control.js';

export function getHtml(this: MarginControlElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="lineContainer">
  <div id="line"></div>
</div>
<div id="row-container">
  <div id="input-container">
    <input id="input" ?disabled="${this.disabled}"
        aria-label="${this.i18n(this.side)}"
        aria-hidden="${this.getAriaHidden_()}"
        @focus="${this.onFocus_}" @blur="${this.onBlur_}"
        data-timeout-delay="1000">
    <span id="unit">${this.measurementSystem?.unitSymbol || ''}</span>
  </div>
  <div id="underline"></div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
