// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SidePanel} from './side_panel.js';

export function getHtml(this: SidePanel) {
  if (!this.showing_) {
    return nothing;
  }

  // clang-format off
  return html`<!--_html_template_start_-->
<div id="frame">
  <div id="header">
  <h2>${this.title_}</h2>
  </div>
  <div id="content">${this.webView}</div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
