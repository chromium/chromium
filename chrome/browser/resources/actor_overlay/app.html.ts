// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ActorOverlayAppElement} from './app.js';

export function getHtml(this: ActorOverlayAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div ?hidden="${!this.borderGlowVisible_}">
  <div id="border-stroke"></div>
  <div id="border-glow"></div>
</div>
<div id="magicCursor"></div>
<!--_html_template_end_-->`;
  // clang-format on
}
