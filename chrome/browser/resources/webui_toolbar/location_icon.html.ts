// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {LocationIconElement} from './location_icon.js';

export function getHtml(this: LocationIconElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<button id="container" type="button"
    aria-label="${this.state.accessibilityState.label}"
    aria-description="${this.state.accessibilityState.description}"
    tabindex="${this.state.isClickable ? '0' : '-1'}"
    @pointerdown="${this.onPointerdown_}"
    @click="${this.onClick_}"
    @contextmenu="${this.onContextmenu_}">
  <icon-from-table .iconHandle="${this.state.icon}">
  </icon-from-table>
  ${this.state.text ? html`<span id="text">${this.state.text}</span>` : ''}
</button>
<!--_html_template_end_-->`;
  // clang-format on
}
