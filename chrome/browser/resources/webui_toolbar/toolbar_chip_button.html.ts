// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {ToolbarChipButtonElement} from './toolbar_chip_button.js';

export function getHtml(this: ToolbarChipButtonElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    <button id="button"
      tabindex="-1"
      ?disabled="${this.disabled}"
      aria-label="${this.ariaLabel}"
      aria-description="${this.ariaDescription}"
      aria-haspopup="${this.ariaHasPopup || nothing}"
      aria-expanded="${this.ariaExpanded || nothing}"
      title="${this.tooltip || ''}">
      <slot name="prefix-icon" @slotchange="${this.onPrefixIconSlotchange_}">
      </slot>
      <slot></slot>
      <slot name="suffix-icon" @slotchange="${this.onSuffixIconSlotchange_}">
      </slot>
    </button>
<!--_html_template_end_-->`;
  // clang-format on
}
