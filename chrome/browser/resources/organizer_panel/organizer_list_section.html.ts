// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OrganizerListSectionElement} from './organizer_list_section.js';

export function getHtml(this: OrganizerListSectionElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<h2 id="header">${this.delegate?.getHeader() || ''}</h2>
<div id="items" role="list">
  ${this.items.map(item => html`
    <organizer-list-section-item .item="${item}" role="listitem">
    </organizer-list-section-item>
  `)}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
