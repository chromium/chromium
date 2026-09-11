// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OrganizerListSectionItemTitleElement} from './organizer_list_section_item_title.js';

export function getHtml(this: OrganizerListSectionItemTitleElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="titleParts" class="title-parts">
  ${this.titleParts.map((titlePart, index) => html`
    ${index > 0 ? html`<div class="separator">|</div>` : ''}
    <div class="title-part" title="${titlePart}">
      ${titlePart}
    </div>
  `)}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
