// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {RawEventDetailsElement} from './raw_event_details.js';

export function getHtml(this: RawEventDetailsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.events.length > 0 ? html`
  <cr-expand-button
      ?expanded="${this.expanded}"
      @expanded-changed="${this.onExpandedChanged}"
      expand-title="${this.label}">
    ${this.label}
  </cr-expand-button>
  <cr-collapse ?opened="${this.expanded}">
    ${this.events.map(item => html`
      <div class="raw-event-json">${JSON.stringify(item, null, 2)}</div>
    `)}
  </cr-collapse>
` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
