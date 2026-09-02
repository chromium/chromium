// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OrganizerListElement} from './organizer_list.js';

export function getHtml(this: OrganizerListElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="sections">
  ${this.sectionDelegates.map((delegate, index) => html`
    ${index > 0 ? html`
      <div class="divider" role="separator" aria-orientation="horizontal"></div>
    ` : ''}
    <organizer-list-section .delegate="${delegate}"
        .searchQuery="${this.searchQuery}">
    </organizer-list-section>
  `)}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
