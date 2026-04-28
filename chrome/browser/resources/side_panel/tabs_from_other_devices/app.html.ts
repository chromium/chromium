// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFaviconForPageURL} from '//resources/js/icon.js';
import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TabInfo, TabsFromOtherDevicesAppElement} from './app.js';

export function getHtml(this: TabsFromOtherDevicesAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="container">
  <cr-toolbar-search-field id="search-input"
      label="$i18n{searchPrompt}"
      @search-changed="${this.onSearchChanged_}"
      ?hidden="${this.syncedDevices_.length === 0}">
  </cr-toolbar-search-field>

  ${this.syncedDevices_.length === 0 ?
      html`<div class="empty-message">$i18n{noSyncedResults}</div>` : ''}

  ${this.searchQuery_ && this.getFilteredTabs_().length === 0 ?
      html`<div class="empty-message">$i18n{noSearchResults}</div>` : ''}

  ${this.syncedDevices_.length > 0 && !this.searchQuery_ ?
      html`
    <div id="picker-container">
      <cr-button id="picker-button" @click="${this.onDeviceSelectClick_}">
        ${this.getSelectedDeviceName_()}
        <cr-icon icon="cr:arrow-drop-down"></cr-icon>
      </cr-button>
      <cr-action-menu id="deviceMenu">
        ${this.syncedDevices_.map(device => html`
          <button class="dropdown-item" @click="${this.onDeviceItemClick_}"
                data-tag="${device.tag}">
            ${device.name}
          </button>
        `)}
      </cr-action-menu>
    </div>
  ` : ''}

  <div id="tabs">
    ${this.getFilteredTabs_().map((tab: TabInfo) => html`
      <div class="tab" @click="${this.onTabClick_}"
          @auxclick="${this.onTabAuxclick_}"
          data-session-tag="${tab.sessionTag}"
          data-tab-id="${tab.sessionId}">
        <div class="tab-favicon-container">
          <div class="tab-favicon"
                style="background-image:
                      ${getFaviconForPageURL(tab.url, true)}">
          </div>
        </div>
        <div class="tab-info">
          <span class="tab-title">${tab.title}</span>
          <div class="tab-details">
            <span class="tab-url">${this.getHostname_(tab.url)}</span>
            <span class="tab-timestamp-separator">&bull;</span>
            <span class="tab-timestamp">${tab.timestampDisplayStr}</span>
          </div>
        </div>
      </div>
    `)}
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
