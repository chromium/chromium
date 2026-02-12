// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {TabStripElement} from './tab_strip.js';

export function getHtml(this: TabStripElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="tabstrip">
    ${this.tabs_.map(item => html`
      <webui-browser-tab id="${this.tabIdToDomId(item.id)}" .data="${item}"
          .dragInProgress="${this.dragInProgress_}"
          @tab-close-click="${this.onTabCloseClick}">
      </webui-browser-tab>
    `)}
  <cr-icon-button id="newTabButton" iron-icon="cr:add"
      title="$i18n{tooltipNewTab}"
      @click="${this.onNewTabButtonClick_}">
  </cr-icon-button>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
