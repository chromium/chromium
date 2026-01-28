// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {TabStrip} from './tab_strip.js';

export function getHtml(this: TabStrip) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="tabstrip">
    ${this.tabs_}
  <cr-icon-button id="newTabButton" iron-icon="cr:add"
    title="$i18n{tooltipNewTab}"
    @click="${this.onAddTab_}"></cr-icon-button>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
