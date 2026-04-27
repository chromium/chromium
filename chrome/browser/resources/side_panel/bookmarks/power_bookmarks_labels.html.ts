// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PowerBookmarksLabelsElement} from './power_bookmarks_labels.js';

export function getHtml(this: PowerBookmarksLabelsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.labels.map((item, index) => html`
  <cr-chip data-index="${index}" ?selected="${item.active}"
      ?disabled="${this.disabled}" @click="${this.onLabelClick}">
    <cr-icon icon="${this.getLabelIcon(item)}"></cr-icon>
    ${item.label}
  </cr-chip>
`)}
<!--_html_template_end_-->`;
  // clang-format on
}
