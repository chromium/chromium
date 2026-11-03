// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CraftAppElement} from './app.js';

export function getHtml(this: CraftAppElement) {
  return html`<!--_html_template_start_-->
<h1>Craft</h1>
<!--_html_template_end_-->`;
}
