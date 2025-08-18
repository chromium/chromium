// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {WebuiBrowserAppElement} from './app.js';

export function getHtml(this: WebuiBrowserAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="activeFrame" id="rootContainer">
  <div id="topContainer">
    <div class="titlebarDiv">
      <div class="tabstripDiv">
        <webui-browser-tabstrip id="tabstrip"
          @tab-click="${this.onTabClick_}"
          @tab-drag-out-of-bounds="${this.onTabDragOutOfBounds_}"
          @tab-close="${this.onTabClosed_}"
          @tab-add="${this.onAddTabClick_}">
        </webui-browser-tabstrip>
      </div>
      <div class="captionButtonsDiv">
        <cr-button type="button" class="caption-button"
          @click="${this.onMinimizeClick_}">
          <cr-icon icon="webui-browser:minimize"></cr-icon>
        </cr-button>
        <cr-button type="button" class="caption-button"
          @click="${this.onMaximizeClick_}">
          <cr-icon icon="webui-browser:maximize"></cr-icon>
        </cr-button>
        <cr-button type="button" class="caption-button"
          @click="${this.onCloseClick_}">
          <cr-icon icon="webui-browser:close"></cr-icon>
          </cr-button>
      </div>
    </div>
    <div id="searchBar">
      <cr-icon-button iron-icon="cr:arrow-back"
        ?disabled="${this.backButtonDisabled_}"
        @click="${this.onBackClick_}"></cr-icon-button>
      <cr-icon-button iron-icon="cr:arrow-forward"
        ?disabled="${this.forwardButtonDisabled_}"
        @click="${this.onForwardClick_}"></cr-icon-button>
      <cr-searchbox id="address"></cr-searchbox>
      <cr-icon-button id="avatarButton" iron-icon="cr:person"
        @click="${this.onAvatarClick_}"></cr-icon-button>
      <cr-icon-button id="appMenuButton" iron-icon="cr:more-vert"
        @click="${this.onAppMenuClick_}"></cr-icon-button>
    </div>
    <webui-browser-bookmark-bar
      id="bookmarkBar"
      @show-bookmark-bar="${this.onShowBookmarkBar_}"
      @hide-bookmark-bar="${this.onHideBookmarkBar_}"
      @bookmark-click="${this.onBookmarkButtonClick_}">
    </webui-browser-bookmark-bar>
  </div>
  <div id="main">
    <content-region id="contentRegion"">
    </content-region>
  </div>
</div>

<!--_html_template_end_-->`;
  // clang-format on
}
