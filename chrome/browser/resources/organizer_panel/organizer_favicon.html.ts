// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OrganizerFaviconElement} from './organizer_favicon.js';

export function getHtml(this: OrganizerFaviconElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="favicons" class="favicons">
  <div id="firstFavicon"
      class="favicon ${this.isNtpUrl_(this.url) ? 'ntp' : ''}"
      .style="${this.getFaviconStyle_(this.url)}">
  </div>
  ${this.secondaryUrl ? html`
    <div id="secondFavicon"
        class="favicon ${this.isNtpUrl_(this.secondaryUrl) ? 'ntp' : ''}"
        .style="${this.getFaviconStyle_(this.secondaryUrl)}">
    </div>
  ` : ''}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
