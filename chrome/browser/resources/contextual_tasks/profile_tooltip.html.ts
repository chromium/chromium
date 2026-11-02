// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ProfileTooltipElement} from './profile_tooltip.js';

export function getHtml(this: ProfileTooltipElement) {
  // clang-format off
  return html`
<div id="tooltip" role="tooltip" ?open="${this.open_}"
    ?unbounded="${this.useUnbounded_}"
    @beforetoggle="${this.onBeforetoggle_}">
  <div class="title">${this.getHeader_()}</div>
  ${this.fullName ? html`<div class="text">${this.fullName}</div>` : ''}
  ${this.email ? html`<div class="text">${this.email}</div>` : ''}
</div>`;
  // clang-format on
}
