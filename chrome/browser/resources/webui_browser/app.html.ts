// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {WebuiBrowserAppElement} from './app.js';

export function getHtml(this: WebuiBrowserAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<h1>WebUI Browser</h1>
<div id="example-div">${this.message_}</div>
<!--_html_template_end_-->`;
  // clang-format on
}
