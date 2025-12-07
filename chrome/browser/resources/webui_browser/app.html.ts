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
    <div class="titlebarDiv" @mousedown="${this.onTabDragMouseDown_}">
      <div class="tabstripDiv" style="margin-left:${this.tabStripInset_}px">
        <webui-browser-tab-strip id="tabstrip"
          @tab-click="${this.onTabClick_}"
          @tab-drag-out-of-bounds="${this.onTabDragOutOfBounds_}"
          @tab-close="${this.onTabClosed_}"
          @tab-add="${this.onAddTabClick_}">
        </webui-browser-tab-strip>
      </div>
      <if expr="not is_macosx">
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
      </if>
    </div>
    <div id="searchBar">
      <cr-icon-button iron-icon="cr:arrow-back"
        ?disabled="${this.backButtonDisabled_}"
        @click="${this.onBackClick_}"></cr-icon-button>
      <cr-icon-button iron-icon="cr:arrow-forward"
        ?disabled="${this.forwardButtonDisabled_}"
        @click="${this.onForwardClick_}"></cr-icon-button>
      <cr-icon-button class="${this.reloadOrStopIcon_}"
        title="${this.reloadOrStopTooltip_()}'"
        @click="${this.onReloadOrStopClick_}"></cr-icon-button>
      <div id="addressBox">
        <cr-searchbox id="address"></cr-searchbox>
        <cr-button id="locationIconButton" type="button"
          ?hidden="${!this.showLocationIconButton_}"
          @click="${this.onLocationIconClick_}">
          <cr-icon id="locationIcon"
            icon="webui-browser:${this.locationIcon_}Icon"></cr-icon>
        </cr-button>
      </div>
      <webui-browser-extensions-bar id="extensionsBar">
      </webui-browser-extensions-bar>
      <cr-icon-button id="avatarButton" iron-icon="cr:person"
        @click="${this.onAvatarClick_}"></cr-icon-button>
      <cr-icon-button id="appMenuButton" iron-icon="cr:more-vert"
        title="$i18n{appMenuTooltip}"
        @click="${this.onAppMenuClick_}"></cr-icon-button>
    </div>
    <webui-browser-bookmark-bar id="bookmarkBar">
    </webui-browser-bookmark-bar>
  </div>
  <div id="main">
    <content-region id="contentRegion"
      ?showing-side-panel="${this.showingSidePanel_}">
    </content-region>
    <side-panel id="sidePanel" @side-panel-closed="${this.onSidePanelClosed_}">
    </side-panel>
  </div>
</div>

<!--_html_template_end_-->`;
  // clang-format on
}
