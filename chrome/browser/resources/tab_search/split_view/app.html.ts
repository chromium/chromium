// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SplitNewTabPageAppElement} from './app.js';

export function getHtml(this: SplitNewTabPageAppElement) {
  return html`<!--_html_template_start_-->
<h1>Split View New Tab Page</h1>
${this.openTabs_.map(data => html`
  <tab-search-item class="mwb-list-item" .data="${data}">
`)}
</tab-search-item>
<!--_html_template_end_-->`;
}
