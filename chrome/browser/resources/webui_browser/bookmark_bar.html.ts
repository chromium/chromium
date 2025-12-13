// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {BookmarkBarElement} from './bookmark_bar.js';

export function getHtml(this: BookmarkBarElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
  ${this.bookmarks_.map((item, index) => html`
    <webui-browser-bookmark-element .data="${item}" data-index="${index}"
        @click="${this.onBookmarkClick_}">
    </webui-browser-bookmark-element>
  `)}
<!--_html_template_end_-->`;
  // clang-format on
}
