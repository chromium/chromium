// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

export function getHtml(settings: string[]) {
  return html`<!--_html_template_start_-->
<cr-frame-list id="cs-page">
  ${settings.map((item, index) => html`
    <div slot="tab" data-page-name="${item.toLowerCase()}"
        ?selected="${index === 0}">
      ${item}
    </div>
    <div slot="panel" class="panel" data-page-name="${item.toLowerCase()}"
        ?hidden="${index !== 0}">
      <div class="main-content-wrapper">
        <div class="panels-container">
          <h2>${item}</h2>
          <div class="content-settings"></div>
        </div>
      </div>
    </div>
  `)}
</cr-frame-list>
<!--_html_template_end_-->`;
}
