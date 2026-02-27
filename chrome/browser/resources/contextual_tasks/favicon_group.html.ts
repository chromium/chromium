// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContextualTasksFaviconGroupElement} from './favicon_group.js';

export function getHtml(this: ContextualTasksFaviconGroupElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
  ${this.visibleItems_.map(item => html`
    ${item.tab ? html`
      <div class="favicon-item" style="background-image: ${
          this.getFaviconUrl_(item.tab.url)}"></div>
    ` : ''}
    ${item.file && !item.tab ? html`
      <cr-icon icon="contextual_tasks:pdf" class="favicon-item file-icon">
      </cr-icon>
    ` : ''}
    ${item.image && !item.tab && !item.file ? html`
      <cr-icon icon="contextual_tasks:img_icon" class="favicon-item">
      </cr-icon>
    ` : ''}
  `)}
  ${this.remainingCount_ > 0 ? html`
    <div id="more-items" class="favicon-item">
      +${this.remainingCount_}
    </div>` : ''}
  <!--_html_template_end_-->`;
  // clang-format on
}
