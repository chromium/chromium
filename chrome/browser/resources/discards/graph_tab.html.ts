// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {GraphTabElement} from './graph_tab.js';

export function getHtml(this: GraphTabElement) {
  //clang-format off
  return html`<!--_html_template_start_-->
<div id="toolTips" width="100%" height="100%"
    @request-node-descriptions="${this.onRequestNodeDescriptions_}">
</div>
<svg id="graphBody" width="100%" height="100%">
  <defs>
    <marker id="arrowToSource" viewBox="0 -5 10 10" refX="-12" refY="0"
            markerWidth="9" markerHeight="6" orient="auto">
      <path d="M15,-7 L0,0 L15,7" >
    </marker>
  </defs>
</svg>
<!--_html_template_end_-->`;
  //clang-format on
}
