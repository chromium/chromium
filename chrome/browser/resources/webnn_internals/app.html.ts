// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {WebnnInternalsAppElement} from './app.js';

export function getHtml(this: WebnnInternalsAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<h1>WebNN Internals</h1>
<cr-tabs id="tabs" .tabNames="${['ML Graph Dump']}"
    .selected="${this.selectedTabIndex_}"
    @selected-changed="${this.onSelectedChanged_}">
</cr-tabs>
<cr-page-selector .selected="${this.selectedTabIndex_}">
<if expr="webnn_enable_graph_dump">
  <webnn-internals-graph-dump class="tab-contents">
  </webnn-internals-graph-dump>
</if>
<if expr="not webnn_enable_graph_dump">
  <div class="text">WebNN graph dump is only supported on debug builds.</div>
</if>
</cr-page-selector>
<!--_html_template_end_-->`;
  // clang-format on
}
