// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContextualTasksFaviconGroupElement} from './favicon_group.js';

export function getHtml(this: ContextualTasksFaviconGroupElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
  ${this.visibleUrls_.map(item => html`
    <div class="favicon-item" style="background-image: ${
        this.getFaviconUrl_(item)}"></div>
  `)}
  ${this.remainingCount_ > 0 ? html`
    <span class="more-items">+${this.remainingCount_}</span>` : ''}
  <!--_html_template_end_-->`;
  // clang-format on
}
