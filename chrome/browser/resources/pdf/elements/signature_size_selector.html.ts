// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SignatureSizeSelectorElement} from './signature_size_selector.js';

export function getHtml(this: SignatureSizeSelectorElement) {
  // TODO(crbug.com/573299995): Replace placeholder strings.
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-icon-button id="decrease" iron-icon="pdf:remove" aria-label="Decrease size"
    title="Decrease size" ?disabled="${this.isDecreaseDisabled_()}"
    @click="${this.onDecreaseClick_}">
</cr-icon-button>
<span id="percentage" aria-live="polite" aria-atomic="true">
  ${this.currentSize}%
</span>
<cr-icon-button id="increase" iron-icon="pdf:add" aria-label="Increase size"
    title="Increase size" ?disabled="${this.isIncreaseDisabled_()}"
    @click="${this.onIncreaseClick_}">
</cr-icon-button>
<!--_html_template_end_-->`;
  // clang-format on
}
