// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OmniboxPopupAppElement} from './app.js';

export function getHtml(this: OmniboxPopupAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-searchbox-dropdown id="matches" .result="${this.result_}"
    ?can-show-secondary-side="${this.canShowSecondarySide}"
    ?has-secondary-side="${this.hasSecondarySide}"
    @has-secondary-side-changed="${this.onHasSecondarySideChanged_}"
    @dom-change="${this.onResultRepaint_}">
</cr-searchbox-dropdown>
<!--_html_template_end_-->`;
  // clang-format on
}
