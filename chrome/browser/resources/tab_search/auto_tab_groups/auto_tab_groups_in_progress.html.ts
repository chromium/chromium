// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AutoTabGroupsInProgressElement} from './auto_tab_groups_in_progress.js';

export function getHtml(this: AutoTabGroupsInProgressElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="loading-container">
  <cr-loading-gradient>
      <svg width="100%" height="191">
        <clipPath>
          <rect x="0" y="0" width="100%" height="35" rx="8"></rect>
          <rect x="0" y="55" width="40" height="40" rx="8"></rect>
          <rect x="56" y="57" width="116" height="16" rx="4"></rect>
          <rect x="56" y="105" width="116" height="16" rx="4"></rect>
          <rect x="56" y="153" width="116" height="16" rx="4"></rect>
          <rect x="56" y="79" width="76" height="14" rx="4"></rect>
          <rect x="56" y="127" width="76" height="14" rx="4"></rect>
          <rect x="56" y="175" width="76" height="14" rx="4"></rect>
          <rect x="0" y="103" width="40" height="40" rx="8"></rect>
          <rect x="0" y="151" width="40" height="40" rx="8"></rect>
        </clipPath>
      </svg>
    </cr-loading-gradient>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
