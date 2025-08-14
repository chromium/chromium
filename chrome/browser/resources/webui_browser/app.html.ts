// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {WebuiBrowserAppElement} from './app.js';

export function getHtml(this: WebuiBrowserAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="activeFrame">
  <div class="titlebarDiv">
    <div class="tabstripDiv">
      <webui-browser-tabstrip
        @tabstrip-added="${this.onTabstripAdded_}"
        @tab-click="${this.onTabClick_}"
        @tab-drag-out-of-bounds="${this.onTabDragOutOfBounds_}"
        @tab-close="${this.onTabClosed_}"
        @tab-add="${this.onAddTabClick_}">
      </webui-browser-tabstrip>
    </div>
    <div class="captionButtonsDiv">
      <cr-button type="button" @click="${this.onMinimizeClick_}">[-]</cr-button>
      <cr-button type="button" @click="${this.onMaximizeClick_}">[+]</cr-button>
      <cr-button type="button" @click="${this.onCloseClick_}">[X]</cr-button>
    </div>
  </div>
  <div id="searchBar">
    <cr-icon-button iron-icon="cr:arrow-back"
      .disabled="${this.backButtonDisabled_}"
      @click="${this.onBackClick_}"></cr-icon-button>
    <cr-icon-button iron-icon="cr:arrow-forward"
      .disabled="${this.forwardButtonDisabled_}"
      @click="${this.onForwardClick_}"></cr-icon-button>
    <cr-searchbox id="address"></cr-searchbox>
  </div>
  <cr-webview id="exampleWebview" guest-id="${this.guestId_}"></cr-webview>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
