// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

export function getHtml(settings: string[]) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-frame-list id="cs-page">
  ${settings.map((setting, index) => html`
    <div slot="tab" data-page-name="${setting.toLowerCase()}"
        ?selected="${index === 0}">
      ${setting}
    </div>
    <div slot="panel" class="panel" data-page-name="${setting.toLowerCase()}"
        ?hidden="${index !== 0}">
      <div class="main-content-wrapper">
        <div class="panels-container">
          <h2>${setting}</h2>
          <div class="content-settings"></div>
        </div>
      </div>
    </div>
  `)}
</cr-frame-list>
<!--_html_template_end_-->`;
  // clang-format on
}
