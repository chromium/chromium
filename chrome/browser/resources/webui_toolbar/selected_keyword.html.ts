// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SelectedKeywordElement} from './selected_keyword.js';

export function getHtml(this: SelectedKeywordElement) {
  // clang-format off
  // TODO(crbug.com/503784002): Show shortName when appropriate, and the icon,
  // as well as draw the separator line.
  return html`<!--_html_template_start_-->
<span>${this.selectedKeywordState?.fullName}</span>
<!--_html_template_end_-->`;
  // clang-format on
}
