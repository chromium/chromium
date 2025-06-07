// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComposeboxFileCarouselElement} from './file_carousel.js';

export function getHtml(this: ComposeboxFileCarouselElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.files.map((item) => {
  if (item.objectUrl) {
    return html`<img class="file"
                    id="${item.uuid}"
                    src="${item.objectUrl}">
                </img>`;
  }
  return html`<p class="file" id="${item.uuid}">${item.name}</p>`;
})}
<!--_html_template_end_-->`;
  // clang-format on
}
