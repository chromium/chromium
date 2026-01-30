// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ImmersiveModeHeaderElement} from './immersive_mode_header.js';

export function getHtml(this: ImmersiveModeHeaderElement) {
  if (!this.isImmersiveEnabled_) {
    return html``;
  }

  // clang-format off
  return html`<!--_html_template_start_-->
<header id="headerContainer" class="read-anything-header">
  <p>$i18n{readAnythingTabTitle}</p>
  <div>
  </div>
</header>
<!--_html_template_end_-->`;
  // clang-format on
}
