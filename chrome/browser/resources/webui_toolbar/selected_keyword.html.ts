// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SelectedKeywordElement} from './selected_keyword.js';

export function getHtml(this: SelectedKeywordElement) {
  // clang-format off
  // TODO(crbug.com/503784002): Tooltip rendering is a little different here.
  // We always show the full one, rather than showing whatever is in use
  // only when truncated.
  return html`<!--_html_template_start_-->
<div id="chip-proper" title="${this.selectedKeywordState.fullName}">
  <icon-from-table .iconHandle="${this.selectedKeywordState.icon}">
  </icon-from-table>
  <div id="text-wrap">
    <div id="short-wrap">
      <span id="short">${this.selectedKeywordState.shortName}</span>
    </div>
    <span id="long">${this.selectedKeywordState.fullName}</span>
  </div>
</div>
<div id="separator"></div>
<!--_html_template_end_-->`;
  // clang-format on
}
