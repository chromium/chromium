// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {IconContainerElement} from './icon_container.js';

export function getHtml(this: IconContainerElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="icons-container">
  ${this.getFaviconCells().map(item => html`
    <div class="cell icon">
      <div class="icon"
          .style="background-image: ${getFaviconForPageURL(item, false)};">
      </div>
    </div>
  `)}
  ${this.getEmptyCells().map(() => html`<div class="cell empty"></div>`)}
  ${this.shouldShowOverflow() ? html`
    <div class="cell overflow-count" aria-hidden="true">
      ${this.getOverflowCount() <= 99 ? html`
        +${this.getOverflowCount()}
      ` : '99+'}
    </div>
  ` : ''}
  </div>
<!--_html_template_end_-->`;
  // clang-format on
}
