// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SearchableLabelElement} from './searchable_label.js';

export function getHtml(this: SearchableLabelElement) {
  return html`<!--_html_template_start_-->
${this.searchTerm ? html`
  ${this.getSegments_().map(item => html`<!--
    -->${item.bold ? html`<!--
      --><b>${item.text}</b><!--
    -->` : html`<!--
      --><span>${item.text}</span><!--
    -->`}<!--
  -->`)}
` : html`
  ${this.title}
`}
<!--_html_template_end_-->`;
}
