// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TabGroupDotElement} from './tab_group_dot.js';

export function getHtml(this: TabGroupDotElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<svg id="groupSvg" viewBox="${this.getViewBox_()}"
    xmlns="http://www.w3.org/2000/svg">
  <circle id="groupDot" cx="0" cy="0" r="${this.getRadius_()}"></circle>
</svg>
<!--_html_template_end_-->`;
  // clang-format on
}
